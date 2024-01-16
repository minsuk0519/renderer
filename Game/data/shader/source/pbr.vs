#include "include\common.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL)
{
    PSInput result;

    //float4 worldPos = mul(obj.objectMat, float4(position, 1.0));
    float4 worldPos = float4(position* 0.5f, 1.0);
    result.worldPos = position;//worldPos.xyz;
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    result.normal = normal;//normalize(mul((float3x3)(obj.objectMat), normal));

    return result;
}

