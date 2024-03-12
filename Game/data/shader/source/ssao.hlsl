#include "include\common.hlsli"
#include "include\packing.hlsli"

Texture2D<float3> positionGbuffer : register(t0);
Texture2D<uint> normalTexGbuffer : register(t1);

RWTexture2D<float> aoBuffer : register(u0);

cbuffer cb_aoConstant : register(b1)
{
	float aoS;
	float aoK;
	float aoRange;
	uint aoNum;
};

[numthreads(8, 8, 1)]
void ssao_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint2 uv = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);
	
	float3 position = positionGbuffer[uv];
	float3 normal = decodeOct(normalTexGbuffer[uv]);
	
	if(normalTexGbuffer[uv] == 0)
	{
		aoBuffer[uv] = 1;
		return;
	}
	normal = normalize(normal);
	
	float3 diff = position - proj.camPos;
	float d = length(diff);
	
	float c = 0.1 * aoRange;
	
	float value = 0;
	uint num = 0;
	for(uint i = 0; i < aoNum; ++i)
	{
		float alpha = (i + 0.5) / aoNum;
		float h = alpha * aoRange / d;
		float phi = (30 * uv.x) ^ uv.y + (10 * uv.x) ^ uv.y;
		float theta = 2 * PI * alpha * (7.0 * aoNum / 9.0) + phi;
		uint2 adjPos = uint2(clamp(uv.x + screenWidth * h * cos(theta), 0, screenWidth - 1), 
			clamp(uv.y + screenHeight * h * sin(theta), 0, screenHeight - 1));
		
		float3 Pi = positionGbuffer[adjPos];
		
		if(normalTexGbuffer[adjPos] == 0)
		{
			continue;
		}
	
		float3 wi = Pi - position;
		
		diff = Pi - proj.camPos;
		float di = length(diff);
		
		float heaviside = ((aoRange - length(wi)) < 0) ? 0 : 1;
		value += max(0, dot(normal, wi) - 0.001 * di) * heaviside / max(c * c, dot(wi, wi));
		++num;
	}
	
	if(num == 0)
	{
		aoBuffer[uv] = 1.0f;

		return;
	}
	
	float S = ((2 * PI * c) / num) * value;
	float A = max(0, pow((1 - aoS * S), aoK));
	
	aoBuffer[uv] = A;
}

RWTexture2D<float> aoBlurBuffer : register(u1);

static float weights[6] = { 
	0.163967222, 
	0.151360810, 
	0.119064637, 
	0.0798114091, 
	0.0455890037, 
	0.0221905485
};

static int w = 5;

void blur_internal(uint2 uv, uint channel)
{
	float3 P = positionGbuffer[uv];
	float3 N = decodeOct(normalTexGbuffer[uv]);
	
	float3 diff = P - proj.camPos;
	float d = length(diff);
	
	float s = 0.01;
	
	float Wsum = 0.0;
	float aoSum = 0.0;
	
	for(int i = -w; i <= w; ++i)
	{
		int2 uvi = uv;
		uv[channel] += i;
		
		if(uvi[channel] >= screenWidth || uvi[channel] < 0)
		{
			continue;
		}
		
		float3 Pi = positionGbuffer[uvi];
		float3 diffi = Pi - proj.camPos;
		float di = length(diffi);
		float3 Ni = decodeOct(normalTexGbuffer[uvi]);
		
		float R = max(0.0, dot(N, Ni)) * exp(-pow(di - d, 2) / (2 * s)) / sqrt(2 * PI * s);
		
		float W = weights[abs(i)] * R;
		
		Wsum += W;
		aoSum += aoBuffer[uvi] * W;
	}
	
	aoSum /= Wsum;
	//if(Wsum >= 0.0000001) aoSum /= Wsum;
			
	aoBlurBuffer[uv] = aoSum;
}


[numthreads(8, 8, 1)]
void blur_horizontal_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint2 uv = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);

	blur_internal(uv, 0);
}

[numthreads(8, 8, 1)]
void blur_vertical_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint2 uv = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);

	blur_internal(uv, 1);
}