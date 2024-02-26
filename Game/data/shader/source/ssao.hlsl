#include "include\common.hlsli"

Texture2D<float3> positionGbuffer : register(t0);
Texture2D<float3> normalTexGbuffer : register(t1);

RWTexture2D<float> aoBuffer : register(u0);

struct aoConstant
{
	float s;
	float k;
	float R;
	int n;
};

[numthreads(8, 8, 1)]
void ssao_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	aoConstant ao;

	ao.s = 1.0f;
	ao.k = 1.0f;
	ao.R = 1.5f;
	ao.n = 10;

	uint2 uv = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);
	
	//float3 position = positionGbuffer.Load3(0, uv);
	//float3 normal = normalTexGbuffer.Load3(0, uv);
	
	float3 position = positionGbuffer[uv];
	float3 normal = normalTexGbuffer[uv];
	
	if(length(normal) == 0.0f)
	{
		aoBuffer[uv] = 0;
		return;
	}
	
	float3 diff = position - proj.camPos;
	float d = length(diff);
	
	float c = 0.1 * ao.R;
	
	float value = 0;
	for(int i = 0; i < ao.n; ++i)
	{
		float alpha = (i + 0.5) / ao.n;
		float h = alpha * ao.R / d;
		float phi = (30 * uv.x) ^ uv.y + (10 * uv.x) ^ uv.y;
		float theta = 2 * PI * alpha * (7.0 * ao.n / 9.0) + phi;
		
		float3 Pi = positionGbuffer[uint2(uv.x + h * cos(theta), uv.y + h * sin(theta))];
	
		float3 wi = Pi - position;
		
		diff = Pi - proj.camPos;
		float di = length(diff);
		
		float heaviside = ((ao.R - length(wi)) < 0) ? 0 : 1;
		value += max(0, dot(normal, wi) - 0.001 * di) * heaviside / max(c * c, dot(wi, wi));
	}
	
	float S = ((2 * PI * c) / ao.n) * value;
	float A = max(0, pow((1 - ao.s * S), ao.k));
	
	aoBuffer[uv] = A;
}