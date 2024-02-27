#include "include\common.hlsli"

struct PSInput
{
	float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
};

struct PSOutput
{
    float4 position     : SV_TARGET0;
    float4 normTex      : SV_TARGET1;
};

PSInput gbuffer_vs(float3 position : POSITION, float3 normal : NORMAL)
{
    PSInput result;

    float4 worldPos = mul(obj.objectMat, float4(position, 1.0));
    result.worldPos = position;//worldPos.xyz;
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    result.normal = normal;//normalize(mul((float3x3)(obj.objectMat), normal));

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
    result.normTex = float4(normal, 1.0f);
    
    return result;
}
