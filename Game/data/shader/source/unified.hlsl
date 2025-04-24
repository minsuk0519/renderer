#include "include\common.hlsli"

RWStructuredBuffer<float3> UVB : register(u0);
RWStructuredBuffer<uint> UIB : register(u1);
//RWStructuredBuffer<float3> clusterBoundingBox : register(u2);

StructuredBuffer<float3> vertexBuffer : register(t0);
StructuredBuffer<uint3> indexBuffer : register(t1);
StructuredBuffer<float3> normalBuffer : register(t2);

ByteAddressBuffer meshInfos : register(t3);

cbuffer cb_unifiedConstant : register(b0)
{
	uint vertexCount;
	uint indexCount;
    uint indexOffset;
    uint meshID;
}

[numthreads(1, 1, 1)]
void unified_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    meshInfo mesh;
    getMeshInfoFromBuffer(meshInfos, meshID, mesh);

    for(uint vertIndex = 0; vertIndex < vertexCount; ++vertIndex)
    {
        UVB[mesh.vertexOffset + vertIndex] = vertexBuffer[vertIndex];
        if(!(mesh.flags & GPU_MESH_INFO_FLAGS_NONORM)) UVB[vertexMax + mesh.vertexOffset + vertIndex] = normalBuffer[vertIndex];
    }

    for(uint id = 0; id < indexCount; ++id)
    {
        UIB[indexOffset + id * 3 + 0] = indexBuffer[id].x;
        UIB[indexOffset + id * 3 + 1] = indexBuffer[id].y;
        UIB[indexOffset + id * 3 + 2] = indexBuffer[id].z;
    }

        // float3 cone_norm;
        // float3 center = (minValue + maxValue) * 0.5f;

        // float t = -9999;
        // float coneOpening = 1;

        // if(length(accumNorm) == 0)
        // {
        //     cone_norm = float3(1,0,0);
        //     coneOpening = 0;
        //     t = 0;
        // }   
        // else
        // {
        //     cone_norm = normalize(accumNorm);

        //     for(j = 0; j < adjustedClusterSize; ++j)
        //     {
        //         uint index = i * clusterSize + j;

        //         float3 normal = norms[j];

        //         const float directionalPart = dot(cone_norm, -normal);

        //         if(directionalPart <= 0)
        //         {
        //             cone_norm = float3(1,0,0);
        //             coneOpening = 0;
        //             t = 0;
        //             break;
        //         }

        //         const float td = dot(center - vert0[j], normal) / -directionalPart;
        //         t = max(t, td);
        //         coneOpening = min(coneOpening, directionalPart);
        //     }
        // }

        // minValue = float3(100,100,100);
        // maxValue = float3(-100,-100,-100);

        // float3 coneCenterPos  = center + cone_norm * t;

        // float coneAngleCosine = sqrt(1 - coneOpening * coneOpening);
		// float coneCenter = Pack3PNForFP32(coneCenterPos);
		// float coneAxis = Pack3PNForFP32(cone_norm);

        // clusterBoundingBox[(clusterOffset + i)] = float3(coneAxis, coneCenter, coneAngleCosine);
}