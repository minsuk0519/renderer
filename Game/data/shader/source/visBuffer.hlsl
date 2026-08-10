#include "include\common.hlsli"
#include "include\packing.hlsli"

Texture2D<uint> visIDGbuffer : register(t0);
Texture2D<float> sceneDepth : register(t1);
ByteAddressBuffer clusterArgs : register(t2);
RWByteAddressBuffer materialPixelCounts : register(u0);
RWByteAddressBuffer materialMemoryOffset : register(u1);
RWByteAddressBuffer materialPixelArgs : register(u2);
RWByteAddressBuffer materialBlockCursor : register(u3);
RWByteAddressBuffer materialPixelInfo : register(u4);

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

        uint laneHits = 0;
        [unroll]
        for (uint j = 0; j < 4; ++j)
        {
            if (matID[j] == waveMin)
            {
                ++laneHits;
                matID[j] = INVALID_ID;
            }
        }

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

[numthreads(8, 4, 1)]
void visBuffer_pixelInfo_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint2 pixel = uint2(groupID.x * 8 + gtid.x, groupID.y * 4 + gtid.y);

    visBufferSample s;
    bool valid = visBuffer_traverse(pixel, s);

    uint matID = valid ? s.objID : INVALID_ID;

    // A block is full iff every lane is covered AND every lane agrees on the material.
    // A fully-covered but mixed-material block is full for neither material.
    uint waveMin = WaveActiveMin(matID);
    uint waveMax = WaveActiveMax(matID);
    bool blockFull = (waveMin == waveMax) && (waveMin != INVALID_ID);

    uint linearPixel = pixel.y * screenWidth + pixel.x;

    // ---- full-block path: ONE atomic reserves all 32 slots ----
    if (blockFull)
    {
        // Wave-uniform branch: matID, waveMin and blockFull are all wave reductions,
        // so WaveIsFirstLane()/WaveReadLaneFirst() are valid here.
        uint base = materialMemoryOffset.Load(matID * 4);

        uint blockStart = 0;
        if (WaveIsFirstLane())
        {
            materialBlockCursor.InterlockedAdd((matID * 2 + 0) * 4, 32, blockStart);
        }
        blockStart = WaveReadLaneFirst(blockStart);

        uint slot = base + blockStart + blockSwizzle8x4(gtid.xy);

        materialPixelInfo.Store2(slot * 8, uint2(linearPixel, s.visID));
    }

    // ---- straggler path: ONE atomic per DISTINCT material in the wave ----
    uint strayMat = (!blockFull && valid) ? matID : INVALID_ID;
    uint strayIdx = 0;

    uint iterCap = WaveGetLaneCount();
    for (uint iter = 0; iter < iterCap; ++iter)
    {
        uint m = WaveActiveMin(strayMat);

        // Wave-uniform: every lane breaks on the same iteration.
        if (m == INVALID_ID)
        {
            break;
        }

        bool isMatch = (strayMat == m);
        uint rank  = WavePrefixCountBits(isMatch);   // this lane's index within the subset
        uint total = WaveActiveCountBits(isMatch);   // subset size, wave-uniform

        uint strayBase = 0;
        if (WaveIsFirstLane())
        {
            materialBlockCursor.InterlockedAdd((m * 2 + 1) * 4, total, strayBase);
        }
        strayBase = WaveReadLaneFirst(strayBase);

        if (isMatch)
        {
            strayIdx = strayBase + rank;
            strayMat = INVALID_ID;                   // retire
        }
    }

    if (!blockFull && valid)
    {
        // Stragglers grow downward from the back of the region.
        uint base  = materialMemoryOffset.Load(matID * 4);
        uint count = materialPixelCounts.Load(matID * 4);

        uint slot = base + count - 1 - strayIdx;

        materialPixelInfo.Store2(slot * 8, uint2(linearPixel, s.visID));
    }
}
