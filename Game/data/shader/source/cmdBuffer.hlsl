#include "include\helper.hlsli"
#include "include\struct.hlsli"

RWStructuredBuffer<cmdBuf> commandBuffer : register(u0);
RWStructuredBuffer<uint3> vertexIDBuffer : register(u1);

StructuredBuffer<meshInfo> meshInfos : register(t0);
StructuredBuffer<lodInfo> lodInfos : register(t1);
StructuredBuffer<clusterInfo> clusterInfos : register(t2);

StructuredBuffer<uint> objectVertexIDOffsets : register(t3);
//StructuredBuffer<bool> clusterVis : register(t1);

cbuffer cb_cmdBuf : register(b0)
{
    //objID, meshIndex
    uint4 obj[MAX_OBJ_NUM / 2];
    uint objCount;
    uint3 pad;
}

//objID, meshIndex, lod
//16 : 13 : 3

static uint2 packedObj[MAX_OBJ_NUM] = (uint2[MAX_OBJ_NUM])obj;  

[numthreads(64, 1, 1)]
void genCmdBuf_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    int localObjectIndex = groupID.x;
    int j = 0;
    int k = 0;

    if(localObjectIndex >= objCount)
    {
        return;
    }

    uint packedID = packedObj[localObjectIndex][0] & ((1 << 16) - 1);
    uint meshIndex = packedID >> 3;
    uint objID = packedObj[localObjectIndex][0] >> 16;

    uint lodIndex = meshInfos[meshIndex].lodOffset + packedID & 0x7;

    uint clusterNum = lodInfos[lodIndex].clusterCount;
    uint clusterOffset = lodInfos[lodIndex].clusterOffset;
    uint totalIndexSize = lodInfos[lodIndex].indexSize;

    uint vertexIDIndex = objectVertexIDOffsets[localObjectIndex];
    
    uint vertexIDBufferNum = 0;
    for(j = 0; j < clusterNum; ++j)
    {
        uint size = clusterInfos[clusterOffset + j].indexSize;

        if(gtid.x * 3 >= size) break;
        //if(clusterVis[packedObj[i][1] + j] == false) continue;
        vertexIDBuffer[vertexIDIndex + vertexIDBufferNum * 192 + gtid.x].x = meshIndex << 24 | 0 << 22 | gtid.x << 16 | j;
        vertexIDBuffer[vertexIDIndex + vertexIDBufferNum * 192 + gtid.x].y = meshIndex << 24 | 1 << 22 | gtid.x << 16 | j;
        vertexIDBuffer[vertexIDIndex + vertexIDBufferNum * 192 + gtid.x].z = meshIndex << 24 | 2 << 22 | gtid.x << 16 | j;


        ++vertexIDBufferNum;
    }

    if(gtid.x == 0)
    {
        commandBuffer[localObjectIndex].cbv = (localObjectIndex << 16) | objID;

        //will be changed
        commandBuffer[localObjectIndex].VertexCountPerInstance = totalIndexSize;
        commandBuffer[localObjectIndex].InstanceCount = 1;
        commandBuffer[localObjectIndex].StartVertexLocation = vertexIDIndex;
        commandBuffer[localObjectIndex].StartInstanceLocation = 0;
    }

    if(gtid.x == 0 && localObjectIndex == 0) commandBuffer[MAX_OBJ_NUM * 2].cbv = objCount;
}