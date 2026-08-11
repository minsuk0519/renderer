#include "include\common.hlsli"
#include "include\packing.hlsli"
#include "include\math.hlsli"

#define GBUFFER_RECORD_THREAD_NUM 32

Texture2D<uint> visIDGbuffer : register(t0);
Texture2D<float> sceneDepth : register(t1);
ByteAddressBuffer clusterArgs : register(t2);
ByteAddressBuffer visibleTris : register(t3);
RWByteAddressBuffer materialPixelCounts : register(u0);
RWByteAddressBuffer materialMemoryOffset : register(u1);
RWByteAddressBuffer materialPixelArgs : register(u2);
RWByteAddressBuffer materialBlockCursor : register(u3);
RWByteAddressBuffer materialPixelInfo : register(u4);
//   [0] = materialID   -- consumed as a root constant by the command signature's leading
//                         D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT desc
//   [1] = ThreadGroupCountX = ceil(materialPixelCounts[m] / GBUFFER_RECORD_THREAD_NUM)
//   [2] = ThreadGroupCountY = 1
//   [3] = ThreadGroupCountZ = 1
RWByteAddressBuffer materialGbufferArgs : register(u5);

RWTexture2D<float4> gbufferPosition : register(u6);   // world-space, float4(worldPos, 1)
RWTexture2D<uint>   gbufferNormal   : register(u7);   // encodeOct(world normal); 0 == no geometry

cbuffer cb_gbufferMaterial : register(b0)
{
    uint gbufferMaterialID;
}

struct visBufferSample
{
    uint clusterSlot;
    uint localTri;
    uint packedID;
    uint objID;
    uint meshIndex;
    uint lod;
    uint visID;
};

bool visBuffer_traverse(uint2 pixel, out visBufferSample result)
{
    result = (visBufferSample)0;

    if (pixel.x >= screenWidth || pixel.y >= screenHeight)
    {
        return false;
    }

    float depth = sceneDepth[pixel];
    if (depth <= 0.0f)
    {
        return false;
    }

    uint visID = visIDGbuffer[pixel];
    uint clusterSlot, localTri;
    decodeVisID(visID, clusterSlot, localTri);

    result.packedID = clusterArgs.Load((clusterSlot * 3 + 2) * 4);

    decodePackedID(result.packedID, result.objID, result.meshIndex, result.lod);

    result.clusterSlot = clusterSlot;
    result.localTri = localTri;
    result.visID = visID;

    return result.objID < MAX_MATERIAL_NUM;
}

[numthreads(64, 1, 1)]
void visBuffer_initMaterialCount_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint threadIndex = gtid.x;
    uint slotsToInitialize = MAX_MATERIAL_NUM;

    for (uint i = threadIndex; i < slotsToInitialize; i += 64)
    {
        materialPixelCounts.Store(i * 4, 0);
        materialBlockCursor.Store2(i * 8, uint2(0, 0));
    }

    if (threadIndex == 0)
    {
        materialPixelArgs.Store4(0, uint4(0, 1, 1, 0));
    }
}

[numthreads(8, 8, 1)]
void visBuffer_materialCount_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint2 quad = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);

    uint matID[4];

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        uint2 p = quad * 2 + uint2(i & 1, i >> 1);

        visBufferSample s;
        if(visBuffer_traverse(p, s))
        {
            matID[i] = s.objID;
        }
        else
        {
            matID[i] = INVALID_ID;
        }
    }

    uint iterCap = 4 * WaveGetLaneCount();
    for (uint iter = 0; iter < iterCap; ++iter)
    {
        uint laneMin = min(min(matID[0], matID[1]), min(matID[2], matID[3]));
        uint waveMin = WaveActiveMin(laneMin);

        if (waveMin == INVALID_ID)
        {
            break;
        }

        uint laneMask = 0;
        [unroll]
        for (uint j = 0; j < 4; ++j)
        {
            if (matID[j] == waveMin)
            {
                laneMask |= (1u << j);
                matID[j] = INVALID_ID;
            }
        }
        uint laneHits = (laneMask != 0) ? 1 : 0;   // one RECORD per quad per material, not one per pixel

        uint waveHits = WaveActiveSum(laneHits);

        if (WaveIsFirstLane())
        {
            uint prev;
            materialPixelCounts.InterlockedAdd(waveMin * 4, waveHits, prev);
        }
    }
}

[numthreads(64, 1, 1)]
void visBuffer_materialOffset_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint materialIndex = groupID.x * 64 + gtid.x;

    uint count = materialPixelCounts.Load(materialIndex * 4);

    uint waveTotal;
    uint offset = waveCompactToBuffer(materialPixelArgs, 0, count, waveTotal);

    materialMemoryOffset.Store(materialIndex * 4, offset);
}

// material present in its quad. NO early return: the wave reductions need every lane.
[numthreads(8, 4, 1)]
void visBuffer_pixelInfo_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint2 quad = uint2(groupID.x * 8 + gtid.x, groupID.y * 4 + gtid.y);
    uint2 topLeft = quad * 2;

    uint matID[4];

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        // Same sub-pixel mapping as visBuffer_materialCount_cs:77 — validMask bit i
        // MUST correspond to this offset.
        uint2 p = topLeft + uint2(i & 1, i >> 1);

        visBufferSample s;
        matID[i] = visBuffer_traverse(p, s) ? s.objID : INVALID_ID;
    }

    // A quad is full iff all 4 pixels are covered and share one material.
    bool quadFull = (matID[0] == matID[1]) && (matID[0] == matID[2]) && (matID[0] == matID[3])
                 && (matID[0] != INVALID_ID);
    uint quadMat = quadFull ? matID[0] : INVALID_ID;

    uint waveMin = WaveActiveMin(quadMat);
    uint waveMax = WaveActiveMax(quadMat);
    bool blockFull = (waveMin == waveMax) && (waveMin != INVALID_ID);

    // ---- full-block path: ONE atomic reserves all 32 records ----
    if (blockFull)
    {
        uint base = materialMemoryOffset.Load(quadMat * 4);

        uint blockStart = 0;
        if (WaveIsFirstLane())
        {
            materialBlockCursor.InterlockedAdd((quadMat * 2 + 0) * 4, 32, blockStart);
        }
        blockStart = WaveReadLaneFirst(blockStart);

        uint slot = base + blockStart + blockSwizzle8x4(gtid.xy);

        materialPixelInfo.Store(slot * 4, encodeQuadRecord(topLeft, 0xF));
    }

    uint pm[4];
    [unroll]
    for (uint k = 0; k < 4; ++k)
    {
        pm[k] = blockFull ? INVALID_ID : matID[k];
    }

    uint iterCap = 4 * WaveGetLaneCount();
    for (uint iter = 0; iter < iterCap; ++iter)
    {
        uint laneMin = min(min(pm[0], pm[1]), min(pm[2], pm[3]));
        uint m = WaveActiveMin(laneMin);

        // Wave-uniform: every lane breaks on the same iteration.
        if (m == INVALID_ID)
        {
            break;
        }

        uint validMask = 0;
        [unroll]
        for (uint j = 0; j < 4; ++j)
        {
            if (pm[j] == m)
            {
                validMask |= (1u << j);
                pm[j] = INVALID_ID;              // retire this material's sub-pixels
            }
        }

        bool isMatch = (validMask != 0);
        uint rank  = WavePrefixCountBits(isMatch);
        uint total = WaveActiveCountBits(isMatch);

        uint strayBase = 0;
        if (WaveIsFirstLane())
        {
            materialBlockCursor.InterlockedAdd((m * 2 + 1) * 4, total, strayBase);
        }
        strayBase = WaveReadLaneFirst(strayBase);

        if (isMatch)
        {
            uint base  = materialMemoryOffset.Load(m * 4);
            uint count = materialPixelCounts.Load(m * 4);

            uint slot = base + count - 1 - (strayBase + rank);

            materialPixelInfo.Store(slot * 4, encodeQuadRecord(topLeft, validMask));
        }
    }
}
[numthreads(64, 1, 1)]
void visBuffer_gbufferArgs_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint materialIndex = groupID.x * 64 + gtid.x;

    uint count = materialPixelCounts.Load(materialIndex * 4);

    // count == 0 yields X == 0: a legal, zero-work indirect dispatch.
    uint groupCount = (count + GBUFFER_RECORD_THREAD_NUM - 1) / GBUFFER_RECORD_THREAD_NUM;

    materialGbufferArgs.Store4(materialIndex * 16, uint4(materialIndex, groupCount, 1, 1));
}

[numthreads(8, 8, 1)]
void visBuffer_gbufferClear_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint2 pixel = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);

    if (pixel.x >= screenWidth || pixel.y >= screenHeight)
    {
        return;
    }

    gbufferPosition[pixel] = float4(0.0f, 0.0f, 0.0f, 0.0f);
    gbufferNormal[pixel]   = 0;
}

struct gbufferTriangle
{
    float3 worldPos[3];
    float3 worldNormal[3];
    float2 screenPos[3];   // pixel space, y-down (viewport convention)
    float  invW[3];        // 1 / clip.w, retained for perspective correction AND for
                           // analytic derivative work built on gbufferBary
};

void fetchGbufferTriangle(uint clusterSlot, uint localTri, uint meshIndex, uint objID,
                          out gbufferTriangle tri)
{
    meshInfo mesh;
    getMeshInfo(meshIndex, mesh);

    viewInfo view;
    getViewInfo(objID, view);

    uint triStart = visibleTris.Load((clusterSlot * 64 + localTri) * 4);

    [unroll]
    for (uint c = 0; c < 3; ++c)
    {
        uint vertexIndex;
        getVertexIndex(triStart + c, vertexIndex);

        float3 localPos;
        getVertex(mesh.vertexOffset + vertexIndex, localPos);
        float3 localNorm;
        getNormal(mesh.vertexOffset + vertexIndex, localNorm);

        float3 worldPos = transformToWorld(view.scale, view.rotation, view.translate, localPos);
        tri.worldPos[c] = worldPos;

        float3 scaledNorm;
        scaledNorm.x = localNorm.x * view.scale.x;
        scaledNorm.y = localNorm.y * view.scale.y;
        scaledNorm.z = localNorm.z * view.scale.z;
        tri.worldNormal[c] = normalize(quatRotate(view.rotation, scaledNorm));

        float4 clipPos = mul(proj.viewProj, float4(worldPos, 1.0f));

        tri.invW[c] = 1.0f / max(clipPos.w, 1e-6f);

        float3 ndc = clipPos.xyz * tri.invW[c];

        tri.screenPos[c] = float2((ndc.x * 0.5f + 0.5f) * (float)screenWidth,
                                  (0.5f - ndc.y * 0.5f) * (float)screenHeight);
    }
}

struct gbufferBary
{
    float3 screenBary;   // affine screen-space barycentrics, sum == 1
    float3 perspBary;    // perspective-correct barycentrics, sum == 1 -- use these to interpolate
    float3 dBaryDx;      // d(screenBary)/d(pixel.x) -- CONSTANT over the triangle
    float3 dBaryDy;      // d(screenBary)/d(pixel.y) -- CONSTANT over the triangle
    bool   valid;        // false if the projected triangle is degenerate
};

void computeGbufferBary(gbufferTriangle tri, float2 pixelCentre, out gbufferBary bary)
{
    float2 e1 = tri.screenPos[1] - tri.screenPos[0];
    float2 e2 = tri.screenPos[2] - tri.screenPos[0];
    float2 ep = pixelCentre      - tri.screenPos[0];

    float det = e1.x * e2.y - e2.x * e1.y;

    bary.valid = abs(det) > 1e-9f;

    float invDet = bary.valid ? (1.0f / det) : 0.0f;

    float b1 = (ep.x * e2.y - e2.x * ep.y) * invDet;
    float b2 = (e1.x * ep.y - ep.x * e1.y) * invDet;

    bary.screenBary = bary.valid ? float3(1.0f - b1 - b2, b1, b2)
                                 : float3(1.0f, 0.0f, 0.0f);

    // d/d(pixel) of b1 and b2; b0 = 1 - b1 - b2 so its gradient is the negated sum.
    float db1dx =  e2.y * invDet;
    float db1dy = -e2.x * invDet;
    float db2dx = -e1.y * invDet;
    float db2dy =  e1.x * invDet;

    bary.dBaryDx = float3(-(db1dx + db2dx), db1dx, db2dx);
    bary.dBaryDy = float3(-(db1dy + db2dy), db1dy, db2dy);

    // Perspective correction: weight the affine barycentrics by 1/w and renormalise.
    float3 pw    = bary.screenBary * float3(tri.invW[0], tri.invW[1], tri.invW[2]);
    float  pwSum = pw.x + pw.y + pw.z;

    bary.perspBary = (abs(pwSum) > 1e-12f) ? (pw / pwSum) : bary.screenBary;
}

[numthreads(GBUFFER_RECORD_THREAD_NUM, 1, 1)]
void visBuffer_gbuffer_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint materialID = gbufferMaterialID;

    uint recordIndex = groupID.x * GBUFFER_RECORD_THREAD_NUM + gtid.x;

    uint count = materialPixelCounts.Load(materialID * 4);

    if (recordIndex >= count)
    {
        return;
    }

    uint base = materialMemoryOffset.Load(materialID * 4);

    uint record = materialPixelInfo.Load((base + recordIndex) * 4);

    uint2 topLeft;
    uint  validMask;
    decodeQuadRecord(record, topLeft, validMask);

    [unroll]
    for (uint j = 0; j < 4; ++j)
    {
        if ((validMask & (1u << j)) == 0)
        {
            continue;
        }

        uint2 pixel = topLeft + uint2(j & 1, j >> 1);

        visBufferSample s;
        if (!visBuffer_traverse(pixel, s))
        {
            continue;
        }

        gbufferTriangle tri;
        fetchGbufferTriangle(s.clusterSlot, s.localTri, s.meshIndex, s.objID, tri);

        gbufferBary bary;
        computeGbufferBary(tri, float2(pixel) + 0.5f, bary);

        if (!bary.valid)
        {
            continue;
        }

        float3 worldPos = bary.perspBary.x * tri.worldPos[0]
                        + bary.perspBary.y * tri.worldPos[1]
                        + bary.perspBary.z * tri.worldPos[2];

        float3 worldNormal = bary.perspBary.x * tri.worldNormal[0]
                           + bary.perspBary.y * tri.worldNormal[1]
                           + bary.perspBary.z * tri.worldNormal[2];

        gbufferPosition[pixel] = float4(worldPos, 1.0f);
        gbufferNormal[pixel]   = encodeOct(normalize(worldNormal));
    }
}
