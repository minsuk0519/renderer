#include "include\common.hlsli"
#include "include\packing.hlsli"

StructuredBuffer<float3> UVB : register(t0);
StructuredBuffer<uint> UIB : register(t1);

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
};

cbuffer cb_objectIdentification : register(b2)
{
    uint objectID;
}

PSInput gbufferIndirect_vs(uint clusterID : VPOSITION)
{
    PSInput result;

    uint clusterNum = ((1 << 16) - 1) & clusterID;
    uint triOffset = ((1 << 6) - 1) & (clusterID >> 16);
    uint triNum = ((1 << 2) - 1 ) & (clusterID >> 22);
    uint meshIndex = clusterID >> 24;
    uint objID = ((1 << 16) - 1) & objectID;

    result.output.x = clusterNum;
    result.output.y = triOffset;
    result.output.z = triNum;
    result.output.w = meshIndex;

    uint vertexIndexOffset = UIB[vertexMax * 3 + meshIndex * 3 + 0];
    uint vertexIndex = UIB[vertexIndexOffset + triNum + 3 * (triOffset + 64 * clusterNum)];

    float3 position = UVB[vertexIndex];
    float3 normal = UVB[vertexIndexOffset + clusterNum * 64 + triOffset + vertexMax];

    float4 worldPos = mul(objs[objID].objectMat, float4(position, 1.0));
    result.worldPos = worldPos.xyz;
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    result.normal = normalize(mul((float3x3)(objs[objID].objectMat), normal));

    return result;
}

PSInput gbuffer_vs(float3 position : VPOSITION, float3 normal : VNORMAL)
{
    PSInput result;

    float4 worldPos = mul(obj.objectMat, float4(position, 1.0));
    result.worldPos = worldPos.xyz;
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    result.normal = normalize(mul((float3x3)(obj.objectMat), normal));

    return result;
}

PSOutput gbuffer_ps(PSInput input)
{
    float3 position = input.worldPos;
	float3 normal = normalize(input.normal);

    PSOutput result;

    //TODO : channel w will be replaced with object ID
    result.position = float4(position, 1.0f);
    //TODO : should pack normal into two channel
    result.normTex = encodeOct(normal);
    
    return result;
}
