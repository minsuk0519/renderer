#include "include\common.hlsli"

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput wireframeAABB_vs(float3 position : POSITION, uint objID : OBJECTID)
{
    PSInput result;

    float4 worldPos = mul(objs[objID].objectMat, float4(position, 1.0));
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    return result;
}

float4 wireframeAABB_ps(PSInput input) : SV_TARGET
{	
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}

PSInput wireframe_vs(float3 position : POSITION)
{
    PSInput result;

    float4 worldPos = float4(position, 1.0);
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    return result;
}

float wireframe_ps(PSInput input) : SV_TARGET
{	
    return 1.0f;
}