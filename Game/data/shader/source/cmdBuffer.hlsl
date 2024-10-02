#include "include\helper.hlsli"
#include "include\cmd.hlsli"

RWStructuredBuffer<cmdBuf> commandBuffer : register(u0);
//RWStructuredBuffer<uint3> clusterBuffer : register(u1);

StructuredBuffer<uint> UIB : register(t0);
//StructuredBuffer<bool> clusterVis : register(t1);

struct object
{
    uint objID;
    uint meshIndex;
};

cbuffer cb_cmdBuf : register(b0)
{
    //objID, meshIndex
    uint4 obj[MAX_OBJ_NUM / 2];
    uint objNum;
}

static uint2 packedObj[MAX_OBJ_NUM] = (uint2[MAX_OBJ_NUM])obj;  

[numthreads(64, 1, 1)]
void genCmdBuf_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    int i = groupID.x;
    int j = 0;
    int k = 0;

    if(i < objNum)
    {
        return;
    }

    uint meshIndex = packedObj[i][0] & ((1 << 16) - 1);
    uint objID = packedObj[i][0] >> 16;

    uint size = UIB[vertexMax * 3 + meshIndex * 3 + 1];
    uint clusterIndexOffset = UIB[vertexMax * 3 + meshIndex * 3 + 2];

    uint clusterNum = (size / 3) / clusterSize;

    uint clusterBufferNum = 0;
    uint vertexCount = 0;
    uint clusterOffset = packedObj[i][1] * clusterSize;
    for(j = 0; j < clusterNum + 1; ++j)
    {
        if(j * clusterSize + gtid.x >= size) break;
        //if(clusterVis[packedObj[i][1] + j] == false) continue;
        //clusterBuffer[clusterOffset + clusterBufferNum * clusterSize + gtid.x].x = meshIndex << 24 | 0 << 22 | gtid.x << 16 | j;
        //clusterBuffer[clusterOffset + clusterBufferNum * clusterSize + gtid.x].y = meshIndex << 24 | 1 << 22 | gtid.x << 16 | j;
        //clusterBuffer[clusterOffset + clusterBufferNum * clusterSize + gtid.x].z = meshIndex << 24 | 2 << 22 | gtid.x << 16 | j;
//
        //++clusterBufferNum;
        if(j == clusterNum) vertexCount += (size % clusterSize);
        else vertexCount += clusterSize * 3;
    }

    if(gtid.x == 0)
    {
        commandBuffer[i].cbv = (i << 16) | objID;

        //will be changed
        commandBuffer[i].VertexCountPerInstance = vertexCount;
        commandBuffer[i].InstanceCount = 1;
        commandBuffer[i].StartVertexLocation = 3 * clusterOffset;
        commandBuffer[i].StartInstanceLocation = 0;
    }

    if(gtid.x == 0 && i == 0) commandBuffer[MAX_OBJ_NUM * 2].cbv = objNum;
}