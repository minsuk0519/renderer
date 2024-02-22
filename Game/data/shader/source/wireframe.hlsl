#include "include\common.hlsli"

struct PSInput
{
	float4 position : SV_POSITION;
};

PSInput wireframe_vs(float3 position : POSITION)
{
    PSInput result;

    float4 worldPos = float4(position * 0.5f, 1.0);
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    return result;
}

float wireframe_ps(PSInput input) : SV_TARGET
{	
    return 1.0f;
}