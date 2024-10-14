#include "include\common.hlsli"

RWStructuredBuffer<float3> UVB : register(u0);
RWStructuredBuffer<uint> UIB : register(u1);
//RWStructuredBuffer<float3> clusterBoundingBox : register(u2);

StructuredBuffer<float3> vertexBuffer : register(t0);
StructuredBuffer<uint3> indexBuffer : register(t1);

cbuffer cb_unifiedConstant : register(b0)
{
	uint clusterOffset;
    uint vertexOffset;
    uint indexOffset;
    uint meshID;
}

[numthreads(1, 1, 1)]
void unified_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    uint vertexNumStructs;
    uint vertexStride;
    vertexBuffer.GetDimensions(vertexNumStructs, vertexStride);
    uint vertexSize = vertexNumStructs / 3;

    uint indexNumStructs;
    uint indexStride;
    indexBuffer.GetDimensions(indexNumStructs, indexStride);
    uint indexSize = indexNumStructs;
    int i;
    int j;

    for(i = 0; i < vertexSize; ++i)
    {
       UVB[vertexOffset + i] = vertexBuffer[i];
    }

    float3 minValue = float3(100,100,100);
    float3 maxValue = float3(-100,-100,-100);

    uint clusterNum = 1 + (indexSize / 3 - 1) / clusterSize;
    
    for(i = 0; i < clusterNum; ++i)
    {
        float3 norms[clusterSize];
        float3 vert0[clusterSize];

        uint adjustedClusterSize = (i == clusterNum - 1) ? (indexSize / 3) % clusterSize : 64;
        if(adjustedClusterSize == 0) adjustedClusterSize = 64;

        float3 accumNorm = float3(0,0,0);
        for(j = 0; j < adjustedClusterSize; ++j)
        {
            uint index = i * clusterSize + j;

            UIB[indexOffset + index * 3 + 0] = vertexOffset + indexBuffer[index].x;
            UIB[indexOffset + index * 3 + 1] = vertexOffset + indexBuffer[index].y;
            UIB[indexOffset + index * 3 + 2] = vertexOffset + indexBuffer[index].z;

            float3 p0 = vertexBuffer[indexBuffer[index].x];
            float3 p1 = vertexBuffer[indexBuffer[index].y];
            float3 p2 = vertexBuffer[indexBuffer[index].z];

            float3 v0 = p1 - p0;
            float3 v1 = p2 - p0;

            minValue.x = min(min(min(p0.x, p1.x), p2.x), minValue.x);
            minValue.y = min(min(min(p0.y, p1.y), p2.y), minValue.y);
            minValue.z = min(min(min(p0.z, p1.z), p2.z), minValue.z);
            maxValue.x = max(max(max(p0.x, p1.x), p2.x), maxValue.x);
            maxValue.y = max(max(max(p0.y, p1.y), p2.y), maxValue.y);
            maxValue.z = max(max(max(p0.z, p1.z), p2.z), maxValue.z);

            float3 normal = normalize(cross(v0, v1));

            norms[j] = normal;
            vert0[j] = p0;

            UVB[vertexMax + indexOffset + index] = normal;
            accumNorm -= normal;
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

    UIB[vertexMax * 3 + meshID * 3 + 0] = indexOffset;
    UIB[vertexMax * 3 + meshID * 3 + 1] = indexSize;
    UIB[vertexMax * 3 + meshID * 3 + 2] = clusterOffset;
}