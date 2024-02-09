#include "include\common.hlsli"

struct PSInput
{
    float4 texCoord : SV_POSITION;
};

PSInput VSMain(float2 position : POSITION)
{
    PSInput result;

    result.texCoord = float4(position, 0.0f, 1.0f);

    return result;
}

