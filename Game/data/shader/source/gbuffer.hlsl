#include "include\common.hlsli"
#include "include\packing.hlsli"
#include "include\math.hlsli"

StructuredBuffer<float3> UVB : register(t0);
StructuredBuffer<uint> UIB : register(t1);
StructuredBuffer<uint> clusterArgs : register(t2);
StructuredBuffer<float> viewInfos : register(t3);
StructuredBuffer<uint> meshInfos : register(t4);

struct PSInput
{
	float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;

    uint4 output : OUTPUT;
};

struct PSOutput
{
    float4 position     : SV_TARGET0;
    uint normTex      	: SV_TARGET1;
    uint debugID      	: SV_TARGET2;
};

cbuffer cb_objectIdentification : register(b2)
{
    uint objectID;
}

PSInput gbufferIndirect_vs(uint vertexID : SV_VertexID, uint clusterID : SV_InstanceID)
{
    PSInput result;

    uint indexOffset = clusterArgs[(clusterID) * 3 + 0];
    uint indexSize = clusterArgs[(clusterID) * 3 + 1];
    uint packedID = clusterArgs[(clusterID) * 3 + 2];

    uint objectInfo = packedID & ((1 << 16) - 1);
    uint meshIndex = objectInfo >> 3;
    uint objID = packedID >> 16;

    uint vertexOffset = meshInfos[meshIndex * 4 + 2];

    uint vertexIndex = UIB[indexOffset + vertexID];

    float3 position = UVB[vertexOffset + vertexIndex];
    float3 normal = UVB[vertexOffset + vertexIndex + vertexMax];

    if(vertexID < indexSize)
    {
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
        float3 worldPos = transformToWorld(scale, rotation, translate, position);
        result.worldPos = worldPos;
        result.position = mul(proj.viewProj, float4(worldPos, 1.0f));

        float3 scaledNorm;
        scaledNorm.x = normal.x * scale.x;
        scaledNorm.y = normal.y * scale.y;
        scaledNorm.z = normal.z * scale.z;
        result.normal = normalize(quatRotate(rotation, scaledNorm));

        result.output = float4(clusterID,meshIndex,objID,vertexIndex);
    }
    return result;
}

PSInput gbuffer_vs(float3 position : VPOSITION, float3 normal : VNORMAL)
{
    PSInput result;

    float4 worldPos = mul(obj.objectMat, float4(position, 1.0));
    result.worldPos = worldPos.xyz;
    result.position = mul(proj.viewProj, worldPos);

    result.output.x = 0;
    result.output.y = 0;
    result.output.z = 0;
    result.output.w = 0;

    result.normal = normalize(mul((float3x3)(obj.objectMat), normal));

    return result;
}

PSOutput gbuffer_ps(PSInput input)
{
    float3 position = input.worldPos;
	float3 normal = normalize(input.normal);

    PSOutput result;

    result.position = float4(position, 1.0f);
    result.normTex = encodeOct(normal);
    result.debugID = input.output.x + 1;//input.output.x;
    
    return result;
}
