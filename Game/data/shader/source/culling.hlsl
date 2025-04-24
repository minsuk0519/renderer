#include "include\helper.hlsli"
#include "include\struct.hlsli"
#include "include\math.hlsli"
#include "include\globalbuffers.hlsli"

RWStructuredBuffer<cmdBufCluster> commandBuffer : register(u0);
RWStructuredBuffer<uint> outClusterArgs : register(u1);
RWStructuredBuffer<uint> localClusterOffsets : register(u2);
RWStructuredBuffer<uint> localClusterSize : register(u3);
RWStructuredBuffer<uint> clsuterCmdBuffer : register(u4);

StructuredBuffer<uint> meshInfos : register(t0);
StructuredBuffer<lodInfo> lodInfos : register(t1);
StructuredBuffer<clusterInfo> clusterInfos : register(t2);
StructuredBuffer<uint> clusterArgs : register(t3);
StructuredBuffer<float> clusterBounds : register(t4);
StructuredBuffer<float> viewInfos : register(t5);

cbuffer cb_cmdBuf : register(b0)
{
    //objID, meshIndex
    uint4 obj[MAX_OBJ_NUM / 4];
    uint objCount;
    uint3 pad;
}

cbuffer cb_cmdBuf : register(b1)
{
    projection proj;
}

//objID, meshIndex, lod
//16 : 13 : 3

static uint packedObj[MAX_OBJ_NUM] = (uint[MAX_OBJ_NUM])obj;

#define CLUSTER_THREAD_NUM 64

[numthreads(1, 1, 1)]
void initCluster_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    for(uint i = 0; i < 12; ++i)
    {
        localClusterSize[i] = 0;
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

    uint lodIndex = meshInfos[meshIndex * 4 + 0] + (packedID & 0x7);

    uint clusterCount = lodInfos[lodIndex].clusterCount;
    uint clusterOffset = lodInfos[lodIndex].clusterOffset;
    uint totalIndexSize = lodInfos[lodIndex].indexSize;

    uint offset;
    InterlockedAdd(localClusterSize[0], clusterCount, offset);
    
    for(uint i = 0; i < clusterCount; ++i)
    {
      localClusterOffsets[(offset + i) * 3 + 0] = lodIndex;
      localClusterOffsets[(offset + i) * 3 + 1] = packedObj[localObjectIndex];
      localClusterOffsets[(offset + i) * 3 + 2] = i;
    }

    GroupMemoryBarrierWithGroupSync();

    if(threadID.x == 0)
    {
        uint totalSize = localClusterSize[0];
        localClusterSize[1] = (1 + (totalSize - 1) / CLUSTER_THREAD_NUM);
        localClusterSize[2] = 1;
        localClusterSize[3] = 1;
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

    if(localClusterSize[0] <= clusterArgsIndex)
    {
        valid = false;
    }

    uint lodIndex = clusterArgs[clusterArgsIndex * 3 + 0];
    uint packedID = clusterArgs[clusterArgsIndex * 3 + 1];
    uint clusterIndex = clusterArgs[clusterArgsIndex * 3 + 2];

    uint clusterCount = lodInfos[lodIndex].clusterCount;
    uint clusterOffset = lodInfos[lodIndex].clusterOffset;
    uint totalIndexSize = lodInfos[lodIndex].indexSize;

    uint indexSize = 0;
    uint indexOffset = 0;

    uint objectInfo = packedID & ((1 << 16) - 1);
    uint meshIndex = objectInfo >> 3;
    uint objID = packedID >> 16;

    float3 sphereCenter = float3(
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 0],
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 1],
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 2]);
    float sphereRadius = 
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 3];
    float3 aabbCenter = float3(
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 4],
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 5],
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 6]);
    float3 aabbhExtent = float3(
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 7],
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 8],
        clusterBounds[(clusterOffset + clusterIndex) * 10 + 9]);

//aabb culling
    bool vis = true;

    if(valid)
    {
        float3 aabbLeftBottom = aabbCenter - aabbhExtent;
        float3 translate = float3(
            viewInfos[objID * 10 + 0],
            viewInfos[objID * 10 + 1],
            viewInfos[objID * 10 + 2]);
        float3 scale = float3(
            viewInfos[objID * 10 + 3],
            viewInfos[objID * 10 + 4],
            viewInfos[objID * 10 + 5]);
        float4 rotation = float4(
            viewInfos[objID * 10 + 6],
            viewInfos[objID * 10 + 7],
            viewInfos[objID * 10 + 8],
            viewInfos[objID * 10 + 9]);
        
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
        indexSize = clusterInfos[clusterOffset + clusterIndex].indexSize;
        indexOffset = clusterInfos[clusterOffset + clusterIndex].indexOffset;
    }

    uint validToInt = valid ? 1 : 0;
    InterlockedAdd(localClusterSize[4], validToInt, offset);

    if(valid)
    {
        outClusterArgs[(offset) * 3 + 0] = indexOffset;
        outClusterArgs[(offset) * 3 + 1] = indexSize;
        outClusterArgs[(offset) * 3 + 2] = packedID;
    }

    GroupMemoryBarrierWithGroupSync();

    if(threadID.x == 0)
    {
        uint totalSize = localClusterSize[4];
        localClusterSize[5] = 3 * CLUSTER_THREAD_NUM;                       //VertexCountPerInstance
        localClusterSize[6] = totalSize;                                    //InstanceCount
        localClusterSize[7] = 0;                                            //StartVertexLocation
        localClusterSize[8] = 0;                                            //StartInstanceLocation
    }
}