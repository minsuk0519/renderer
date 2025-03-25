#include "include\helper.hlsli"
#include "include\struct.hlsli"

RWStructuredBuffer<cmdBufCluster> commandBuffer : register(u0);
RWStructuredBuffer<uint> outClusterArgs : register(u1);
RWStructuredBuffer<uint> localClusterOffsets : register(u2);
RWStructuredBuffer<uint> localClusterSize : register(u3);
RWStructuredBuffer<uint> clsuterCmdBufferclsuterCmdBuffer : register(u4);

StructuredBuffer<uint> meshInfos : register(t0);
StructuredBuffer<lodInfo> lodInfos : register(t1);
StructuredBuffer<clusterInfo> clusterInfos : register(t2);
StructuredBuffer<uint> clusterArgs : register(t3);

cbuffer cb_cmdBuf : register(b0)
{
    //objID, meshIndex
    uint4 obj[MAX_OBJ_NUM / 4];
    uint objCount;
    uint3 pad;
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

[numthreads(CLUSTER_THREAD_NUM, 1, 1)]
void cullCluster_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    bool vis = true;

    uint clusterArgsIndex = (groupID.x * CLUSTER_THREAD_NUM + gtid.x);

    if(localClusterSize[0] <= clusterArgsIndex)
    {
        vis = false;
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

    uint offset = 0;
    if(vis)
    {
        indexSize = clusterInfos[clusterOffset + clusterIndex].indexSize;
        indexOffset = clusterInfos[clusterOffset + clusterIndex].indexOffset;
    }

    uint visToInt = vis ? 1 : 0;
    InterlockedAdd(localClusterSize[4], visToInt, offset);
    offset = clusterArgsIndex;

    if(vis)
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