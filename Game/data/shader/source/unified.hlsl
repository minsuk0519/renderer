#include "include\common.hlsli"

RWByteAddressBuffer writeUVB : register(u0);
RWByteAddressBuffer writeUIB : register(u1);
//RWByteAddressBuffer clusterBoundingBox : register(u2);

ByteAddressBuffer vertexBuffer : register(t0);
ByteAddressBuffer indexBuffer : register(t1);
ByteAddressBuffer normalBuffer : register(t2);

cbuffer cb_unifiedConstant : register(b0)
{
	uint vertexCount;
	uint indexCount;
    uint indexOffset;
    uint vertexOffset;
}

//deprecated.
//may be used in generating cone bounds for future use? 
[numthreads(1, 1, 1)]
void unified_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    for(uint vertIndex = 0; vertIndex < vertexCount; ++vertIndex)
    {
        writeUVB.Store3((vertexOffset + vertIndex) * VERTEX_STRUCT_SIZE, vertexBuffer.Load3(vertIndex * VERTEX_STRUCT_SIZE));
        writeUVB.Store3((vertexMax + vertexOffset + vertIndex) * VERTEX_STRUCT_SIZE, normalBuffer.Load3(vertIndex * VERTEX_STRUCT_SIZE));
    }

    for(uint id = 0; id < indexCount; ++id)
    {
        writeUIB.Store3((indexOffset + id) * INDEX_STRUCT_SIZE, indexBuffer.Load3(id * INDEX_STRUCT_SIZE));
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