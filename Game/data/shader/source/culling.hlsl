#include "include\common.hlsli"
#include "include\math.hlsli"

RWByteAddressBuffer commandBuffer : register(u0);
RWByteAddressBuffer outClusterArgs : register(u1);
RWByteAddressBuffer localClusterOffsets : register(u2);
RWByteAddressBuffer localClusterSize : register(u3);
RWByteAddressBuffer clsuterCmdBuffer : register(u4);

ByteAddressBuffer clusterArgs : register(t0);

#define CLUSTER_THREAD_NUM 64

[numthreads(1, 1, 1)]
void initCluster_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    for(uint i = 0; i < 12; ++i)
    {
        localClusterSize.Store(i * 4, 0);
    }
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

    uint offset;
    localClusterSize.InterlockedAdd(0, clusterCount, offset);
    
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

[numthreads(CLUSTER_THREAD_NUM, 1, 1)]
void cullCluster_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    bool valid = true;

    uint clusterArgsIndex = (groupID.x * CLUSTER_THREAD_NUM + gtid.x);

    uint localClusterSizeValue = localClusterSize.Load(0);

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

    valid = valid && vis;

    uint offset = 0;
    if(valid)
    {
        clusterInfo clusters;
        getClusterInfo(clusterOffset + clusterIndex, clusters);
        indexSize = clusters.indexSize;
        indexOffset = clusters.indexOffset;
    }

    uint validToInt = valid ? 1 : 0;
    localClusterSize.InterlockedAdd(4 * 4, validToInt, offset);

    if(valid)
    {
        outClusterArgs.Store3(offset * 4 * 3, uint3(indexOffset, indexSize, packedID));
    }

    GroupMemoryBarrierWithGroupSync();

    if(threadID.x == 0)
    {
        uint totalSize = localClusterSize.Load(4 * 4);
        localClusterSize.Store4(5 * 4, uint4(
                3 * CLUSTER_THREAD_NUM,                       //VertexCountPerInstance
                totalSize,                                    //InstanceCount
                0,                                            //StartVertexLocation
                0                                             //StartInstanceLocation
        ));
    }
}