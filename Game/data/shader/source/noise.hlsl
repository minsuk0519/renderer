#include "include\helper.hlsli"

RWStructuredBuffer<float> noiseOut : register(u0);

cbuffer cb_noiseConstant : register(b0)
{
    uint octavesConst;
    float zConst;
}

[numthreads(1, 1, 1)]
void noise_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	float x = (float)groupID.x;
	float y = (float)groupID.y;

	noiseOut[y * NOISE_WIDTH + x] = OctavePerlin(x / (float)NOISE_WIDTH, zConst, y / (float)NOISE_HEIGHT, octavesConst);
}