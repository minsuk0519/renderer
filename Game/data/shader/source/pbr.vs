#include "include\common.hlsli"

struct PSInput
{
    float4 position 	: SV_POSITION;
    float2 texCoord 	: TEXTURE_COORD;
};

PSInput VSMain(float2 position : POSITION)
{
    PSInput result;

    result.position = float4(position, 0.0f, 1.0f);
    result.texCoord = float2(position.x, -position.y);

    return result;
}

