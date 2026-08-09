#include "include\common.hlsli"
#include "include\math.hlsli"

RWByteAddressBuffer commandBuffer : register(u0);
RWByteAddressBuffer outClusterArgs : register(u1);
RWByteAddressBuffer localClusterOffsets : register(u2);
RWByteAddressBuffer localClusterSize : register(u3);
RWByteAddressBuffer clsuterCmdBuffer : register(u4);
RWByteAddressBuffer outVisibleTris : register(u5);
RWByteAddressBuffer outOccludedClusters : register(u6);

ByteAddressBuffer clusterArgs : register(t0);

Texture2D<float> hzb : register(t1);

cbuffer cb_hzbCull : register(b0)
{
    uint hzbWidth;
    uint hzbHeight;
    uint hzbMipCount;
    uint hzbCullEnable;
};

#define HZB_SAMPLING_PIXEL_SIZE 4

#define CLUSTER_THREAD_NUM 64

int calculateMip(int4 pixelRect)
{
    const int maxTexelOffset = HZB_SAMPLING_PIXEL_SIZE - 1;
    const int mipBias = (int)log2((float)HZB_SAMPLING_PIXEL_SIZE) - 1;
    int2 axisMips = firstbithigh(pixelRect.zw - pixelRect.xy);
    int mip = max(max(axisMips.x, axisMips.y) - mipBias, 0);
    mip += any((pixelRect.zw >> mip) - (pixelRect.xy >> mip) > maxTexelOffset) ? 1 : 0;
    return mip;
}

[numthreads(1, 1, 1)]
void initCluster_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    for(uint i = 0; i < 16; ++i)
    {
        localClusterSize.Store(i * 4, 0);
    }

    localClusterSize.Store2(10 * 4, uint2(1, 1));
}


[numthreads(CLUSTER_THREAD_NUM, 1, 1)]
void uploadLocalObj_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    int localObjectIndex = gtid.x + groupID.x * CLUSTER_THREAD_NUM;
    int j = 0;
    int k = 0;

    if(localObjectIndex >= objCount)
    {
        return;
    }

    uint packedID = packedObj[localObjectIndex] & ((1 << 16) - 1);
    uint meshIndex = packedID >> 3;
    uint objID = packedObj[localObjectIndex] >> 16;
    
    meshInfo mesh;
    getMeshInfo(meshIndex, mesh);
    uint lodIndex = mesh.lodOffset + (packedID & 0x7);

    lodInfo lod;
    getLODInfo(lodIndex, lod);
    uint clusterCount = lod.clusterCount;
    uint clusterOffset = lod.clusterOffset;
    uint totalIndexSize = lod.indexSize;

    uint waveTotal;
    uint offset = waveCompactToBuffer(localClusterSize, 0, clusterCount, waveTotal);

    for(uint i = 0; i < clusterCount; ++i)
    {
        localClusterOffsets.Store3((offset + i) * 4 * 3, uint3(lodIndex, packedObj[localObjectIndex], i));
    }

    GroupMemoryBarrierWithGroupSync();

    if(threadID.x == 0)
    {
        uint totalSize = localClusterSize.Load(0);
        localClusterSize.Store3(4, uint3((1 + (totalSize - 1) / CLUSTER_THREAD_NUM), 1, 1));
    }
}

void setSideFlag(float4 pos, inout uint flag)
{
    float zValue = pos.z / pos.w;

    float2 clipCoord = pos.xy / pos.w;

    flag |= (clipCoord.x < -1.0f) ? (1 << 0) : ((clipCoord.x > 1.0f) ? (1 << 2) : (1 << 1));
    flag |= (clipCoord.y < -1.0f) ? (1 << 3) : ((clipCoord.y > 1.0f) ? (1 << 5) : (1 << 4));
    flag |= (zValue > 1.0f) ? (1 << 6) : (1 << 7);
}

void cullCluster_internal(uint3 groupID, uint3 gtid, uint threadID, bool isPost)
{
    bool valid = true;

    uint clusterArgsIndex = (groupID.x * CLUSTER_THREAD_NUM + gtid.x);

    uint localClusterSizeValue = isPost ? localClusterSize.Load(12 * 4) : localClusterSize.Load(0);

    if(localClusterSizeValue <= clusterArgsIndex)
    {
        valid = false;
    }

    uint3 clusterArg = clusterArgs.Load3(clusterArgsIndex * 4 * 3);

    uint lodIndex = clusterArg.x;
    uint packedID = clusterArg.y;
    uint clusterIndex = clusterArg.z;

    lodInfo lod;
    getLODInfo(lodIndex, lod);
    uint clusterCount = lod.clusterCount;
    uint clusterOffset = lod.clusterOffset;
    uint totalIndexSize = lod.indexSize;

    uint indexSize = 0;
    uint indexOffset = 0;

    uint objectInfo = packedID & ((1 << 16) - 1);
    uint meshIndex = objectInfo >> 3;
    uint objID = packedID >> 16;

    clusterBound clusterbound;
    getClusterBound(clusterOffset + clusterIndex, clusterbound);
    float3 sphereCenter = clusterbound.sphereCenter;
    float sphereRadius = clusterbound.sphereRadius;
    float3 aabbCenter = clusterbound.aabbCenter;
    float3 aabbhExtent = clusterbound.aabbhExtent;

//aabb culling
    bool vis = true;
    bool occludedByHZB = false;

    if(valid)
    {
        float3 aabbLeftBottom = aabbCenter - aabbhExtent;
        viewInfo view;
        getViewInfo(objID, view);
        float3 translate = view.translate;
        float3 scale = view.scale;
        float4 rotation = view.rotation;

        float3 LB = transformToWorld(scale, rotation, translate, aabbLeftBottom);

        //get up right front vector from quat
        float3 right, up, forward;
        getAxisVecFromQuat(rotation, right, up, forward);
        right *= aabbhExtent.x * scale.x * 2.0f;
        up *= aabbhExtent.y * scale.y * 2.0f;
        forward *= aabbhExtent.z * scale.z * 2.0f;

        if(!isPost)
        {
            float4 startPos = mul(proj.viewProj, float4(LB, 1.0f));
            float4 debug = startPos;

            float4 deltaX = mul(proj.viewProj, float4(right, 0.0f));
            float4 deltaY = mul(proj.viewProj, float4(up, 0.0f));
            float4 deltaZ = mul(proj.viewProj, float4(forward, 0.0f));

            float2 clipCoord = startPos.xy / startPos.w;
            //0-2 left mid right
            //3-5 up mid down
            //6-7 back front
            uint sideFlag = 0;
            setSideFlag(startPos, sideFlag);
            startPos += deltaX;
            setSideFlag(startPos, sideFlag);
            startPos += deltaY;
            setSideFlag(startPos, sideFlag);
            startPos -= deltaX;
            setSideFlag(startPos, sideFlag);
            startPos += deltaZ;
            setSideFlag(startPos, sideFlag);
            startPos += deltaX;
            setSideFlag(startPos, sideFlag);
            startPos -= deltaY;
            setSideFlag(startPos, sideFlag);
            startPos -= deltaX;

            //fully behind
            if(!(sideFlag & (1 << 7))) vis = false;
            else
            {
                if(!(sideFlag & (1 << 1)))
                {
                    if(!(sideFlag & (1 << 0)) || !(sideFlag & (1 << 2))) vis = false;
                }
                else if(!(sideFlag & (1 << 4)))
                {
                    if(!(sideFlag & (1 << 3)) || !(sideFlag & (1 << 5))) vis = false;
                }
            }
        }

        if(vis && hzbCullEnable != 0)
        {
            float4x4 reprojMat = isPost ? proj.viewProj : proj.prevViewProj;

            float4 corners[8];
            corners[0] = mul(reprojMat, float4(LB, 1.0f));
            corners[1] = mul(reprojMat, float4(LB + right, 1.0f));
            corners[2] = mul(reprojMat, float4(LB + up, 1.0f));
            corners[3] = mul(reprojMat, float4(LB + right + up, 1.0f));
            corners[4] = mul(reprojMat, float4(LB + forward, 1.0f));
            corners[5] = mul(reprojMat, float4(LB + forward + right, 1.0f));
            corners[6] = mul(reprojMat, float4(LB + forward + up, 1.0f));
            corners[7] = mul(reprojMat, float4(LB + forward + right + up, 1.0f));

            bool nearPlaneCross = false;
            for(uint c = 0; c < 8; ++c)
            {
                if(corners[c].w <= 0.0f) nearPlaneCross = true;
            }

            if(!nearPlaneCross)
            {
                float minX = 1e30f, minY = 1e30f;
                float maxX = -1e30f, maxY = -1e30f;
                float clusterNearestZ = -1e30f;

                for(uint c2 = 0; c2 < 8; ++c2)
                {
                    float2 ndc = corners[c2].xy / corners[c2].w;
                    float z = corners[c2].z / corners[c2].w;

                    float px = (ndc.x * 0.5f + 0.5f) * hzbWidth;
                    float py = (0.5f - ndc.y * 0.5f) * hzbHeight;

                    minX = min(minX, px);
                    minY = min(minY, py);
                    maxX = max(maxX, px);
                    maxY = max(maxY, py);

                    clusterNearestZ = max(clusterNearestZ, z);
                }

                if(maxX >= 0.0f && maxY >= 0.0f && minX < (float)hzbWidth && minY < (float)hzbHeight)
                {
                    int4 rect = int4(floor(minX), floor(minY), ceil(maxX), ceil(maxY));

                    rect.x = clamp(rect.x, 0, (int)hzbWidth - 1);
                    rect.y = clamp(rect.y, 0, (int)hzbHeight - 1);
                    rect.z = clamp(rect.z, 0, (int)hzbWidth - 1);
                    rect.w = clamp(rect.w, 0, (int)hzbHeight - 1);

                    rect.z = max(rect.z, rect.x);
                    rect.w = max(rect.w, rect.y);

                    int mip = calculateMip(rect);
                    mip = min(mip, (int)hzbMipCount - 1);

                    uint mipW = max(1u, hzbWidth >> mip);
                    uint mipH = max(1u, hzbHeight >> mip);

                    int2 texelMin = rect.xy >> mip;
                    int2 texelMax = rect.zw >> mip;

                    texelMin.x = clamp(texelMin.x, 0, (int)mipW - 1);
                    texelMin.y = clamp(texelMin.y, 0, (int)mipH - 1);
                    texelMax.x = clamp(texelMax.x, 0, (int)mipW - 1);
                    texelMax.y = clamp(texelMax.y, 0, (int)mipH - 1);

                    float hzbFarthestZ = 1e30f;
                    for(int ty = texelMin.y; ty <= texelMax.y; ++ty)
                    {
                        for(int tx = texelMin.x; tx <= texelMax.x; ++tx)
                        {
                            hzbFarthestZ = min(hzbFarthestZ, hzb.Load(int3(tx, ty, mip)));
                        }
                    }

                    if(clusterNearestZ < hzbFarthestZ) { vis = false; occludedByHZB = true; }
                }
            }
        }
    }

    valid = valid && vis;

    if (!isPost)
    {
        uint hzbOccludedCount = WaveActiveCountBits(occludedByHZB);
        if (WaveIsFirstLane() && hzbOccludedCount > 0)
        {
            uint prevValue;
            localClusterSize.InterlockedAdd(4 * 4, hzbOccludedCount, prevValue);
        }

        uint occludedWaveCount;
        uint occludedOffset = waveCompactToBuffer(localClusterSize, 12 * 4, occludedByHZB ? 1 : 0, occludedWaveCount);
        if (occludedByHZB)
        {
            outOccludedClusters.Store3(occludedOffset * 4 * 3, uint3(lodIndex, packedID, clusterIndex));
        }
    }

    uint offset = 0;
    if(valid)
    {
        clusterInfo clusters;
        getClusterInfo(clusterOffset + clusterIndex, clusters);
        indexSize = clusters.indexSize;
        indexOffset = clusters.indexOffset;
    }

    uint waveClusterCount;
    offset = waveCompactToBuffer(localClusterSize, 9 * 4, valid ? 1 : 0, waveClusterCount);

    if(valid)
    {
        outClusterArgs.Store3(offset * 4 * 3, uint3(indexOffset, indexSize, packedID));
    }
}

[numthreads(CLUSTER_THREAD_NUM, 1, 1)]
void cullCluster_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex)
{
    cullCluster_internal(groupID, gtid, threadID, false);
}

[numthreads(CLUSTER_THREAD_NUM, 1, 1)]
void cullCluster_post_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex)
{
    cullCluster_internal(groupID, gtid, threadID, true);
}

[numthreads(1, 1, 1)]
void prepPostArgs_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex)
{
    uint occludedCount = localClusterSize.Load(12 * 4);
    localClusterSize.Store3(13 * 4, uint3((occludedCount + CLUSTER_THREAD_NUM - 1) / CLUSTER_THREAD_NUM, 1, 1));

    localClusterSize.Store(9 * 4, 0);
    localClusterSize.Store4(5 * 4, uint4(0, 0, 0, 0));
}

groupshared uint gsWaveCounts[2];

[numthreads(CLUSTER_THREAD_NUM, 1, 1)]
void rasterizer_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    bool valid = true;

    uint survivorCount = localClusterSize.Load(9 * 4);

    if(groupID.x >= survivorCount)
    {
        valid = false;
    }

    uint3 rec = uint3(0, 0, 0);
    if(valid)
    {
        rec = outClusterArgs.Load3(groupID.x * 4 * 3);
    }

    uint indexOffset = rec.x;
    uint indexSize = min(rec.y, CLUSTER_THREAD_NUM * 3);
    uint packedID = rec.z;

    uint t = gtid.x;

    valid = valid && (t * 3 + 2 < indexSize);

    bool visible = false;
    uint firstIndex = 0;

    if(valid)
    {
        uint meshIndex = (packedID & 0xFFFF) >> 3;
        uint objID = packedID >> 16;

        meshInfo mesh;
        getMeshInfo(meshIndex, mesh);

        viewInfo view;
        getViewInfo(objID, view);
        float3 translate = view.translate;
        float3 scale = view.scale;
        float4 rotation = view.rotation;

        firstIndex = indexOffset + t * 3;

        float4 clip[3];
        bool nearPlaneInvalid = false;

        for(uint i = 0; i < 3; ++i)
        {
            uint vi;
            getVertexIndex(firstIndex + i, vi);

            float3 pos;
            getVertex(mesh.vertexOffset + vi, pos);

            float3 worldPos = transformToWorld(scale, rotation, translate, pos);

            clip[i] = mul(proj.viewProj, float4(worldPos, 1.0f));

            if(clip[i].w <= 0.0f)
            {
                nearPlaneInvalid = true;
            }
        }

        if(nearPlaneInvalid)
        {
            visible = true;
        }
        else
        {
            float2 s[3];
            for(uint j = 0; j < 3; ++j)
            {
                float2 ndc = clip[j].xy / clip[j].w;
                s[j] = float2((ndc.x * 0.5f + 0.5f) * screenWidth, (0.5f - ndc.y * 0.5f) * screenHeight);
            }

            float area2 = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);

            bool backFacing = area2 >= 0.0f;

            float minX = min(s[0].x, min(s[1].x, s[2].x));
            float maxX = max(s[0].x, max(s[1].x, s[2].x));
            float minY = min(s[0].y, min(s[1].y, s[2].y));
            float maxY = max(s[0].y, max(s[1].y, s[2].y));

            bool subPixel = (ceil(minX - 0.5f) > floor(maxX - 0.5f)) || (ceil(minY - 0.5f) > floor(maxY - 0.5f));

            visible = !backFacing && !subPixel;
        }
    }

    //hardcoded 32: target hardware is wave32 (see gsWaveCounts comment)
    uint waveIndex = threadID.x / 32;

    uint visibleToInt = visible ? 1 : 0;
    uint waveLocalOffset = WavePrefixSum(visibleToInt);
    uint waveTotal = WaveActiveSum(visibleToInt);

    if (WaveIsFirstLane())
    {
        gsWaveCounts[waveIndex] = waveTotal;
    }

    GroupMemoryBarrierWithGroupSync();

    uint indexTotal = gsWaveCounts[0] + gsWaveCounts[1];
    uint slot = (waveIndex == 0 ? 0 : gsWaveCounts[0]) + waveLocalOffset;

    if (visible)
    {
        outVisibleTris.Store((groupID.x * CLUSTER_THREAD_NUM + slot) * 4, firstIndex);
    }

    if (threadID.x == 0 && groupID.x < survivorCount)
    {
        outClusterArgs.Store((groupID.x * 3 + 1) * 4, indexTotal * 3);
    }

    if(threadID.x == 0 && groupID.x == 0)
    {
        localClusterSize.Store4(5 * 4, uint4(
                3 * CLUSTER_THREAD_NUM,   //VertexCountPerInstance
                survivorCount,            //InstanceCount 
                0,                        //StartVertexLocation
                0                         //StartInstanceLocation
        ));
    }
}