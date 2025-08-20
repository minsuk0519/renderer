//DEPRECATED

#include "include\common.hlsli"

RWByteAddressBuffer commandBuffer : register(u0);
RWByteAddressBuffer vertexIDBuffer : register(u1);

ByteAddressBuffer objectVertexIDOffsets : register(t2);
ByteAddressBuffer totalClusterSize : register(t3);
ByteAddressBuffer clusterVis : register(t4);

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

    uint packedID = packedObj[localObjectIndex] & ((1 << 16) - 1);
    uint meshIndex = packedID >> 3;
    uint objID = packedObj[localObjectIndex] >> 16;

    meshInfo mesh;
    getMeshInfo(meshIndex, mesh);
    uint lodIndex = mesh.lodOffset + packedID & 0x7;
    
    lodInfo lod;
    getLODInfo(lodIndex, lod);

    uint vertexIDIndex = objectVertexIDOffsets.Load(localObjectIndex * 4);
    
    uint vertexIDBufferNum = 0;
    for(j = 0; j < 3; ++j)
    {
        clusterInfo clusters;
        getClusterInfo(lod.clusterOffset + j, clusters);
        uint size = clusters.indexSize;

        if(gtid.x * 3 >= size) break;
        //if(clusterVis[packedObj[i][1] + j] == false) continue;
        vertexIDBuffer.Store3((vertexIDIndex + vertexIDBufferNum * 192 + gtid.x) * 4 * 4, uint3(
            meshIndex << 24 | 0 << 22 | gtid.x << 16 | j, meshIndex << 24 | 1 << 22 | gtid.x << 16 | j, meshIndex << 24 | 2 << 22 | gtid.x << 16 | j));

        ++vertexIDBufferNum;
    }

    if(gtid.x == 0)
    {
        commandBuffer.Store((localObjectIndex * 5 + 0) * 4, (localObjectIndex << 16) | objID);

        //will be changed
        commandBuffer.Store4((localObjectIndex * 5 + 1) * 4, uint4(lod.indexSize, 1, vertexIDIndex, 0));
    }

    if(gtid.x == 0 && localObjectIndex == 0) commandBuffer.Store(MAX_OBJ_NUM * 2 * 4, objCount);
}