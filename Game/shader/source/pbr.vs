#include "include\common.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    uint4 output : OUTPUT;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL)
{
    PSInput result;

    float4 worldPos = mul(obj.objectMat, float4(position, 1.0));
    result.worldPos = worldPos.xyz;
    float4 viewPos = mul(proj.viewMat, worldPos);
    result.position = mul(proj.projectionMat, viewPos);

    result.normal = normalize(mul((float3x3)(obj.objectMat), normal));

    return result;
}

