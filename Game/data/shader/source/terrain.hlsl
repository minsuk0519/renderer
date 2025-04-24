#include "include\common.hlsli"

RWStructuredBuffer<float3> vertOut : register(u0);
RWStructuredBuffer<float3> normOut : register(u1);
RWStructuredBuffer<uint3> indexOut : register(u2);

StructuredBuffer<float> noiseMap : register(t0);

static const int THREADS_X = 8;
static const int THREADS_Y = 8;

static const int NUMTHREADS = THREADS_X * THREADS_Y;

cbuffer cb_Height : register(b0)
{
	float HEIGHT_MODIFIER = 100.0f;
}

groupshared float HEIGHTS[THREADS_Y][THREADS_X];

uint getIndex(uint u, uint v)
{
	return u + v * NOISE_WIDTH;
}

[numthreads(THREADS_X, THREADS_Y, 1)]
void genTerrainVert_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint u = groupID.x * (THREADS_X - 1) + gtid.x;
	uint v = groupID.y * (THREADS_Y - 1) + gtid.y;

	bool skipVert = false;

	if((u != 0 && gtid.x == 0) || (v != 0 && gtid.y == 0)) skipVert = true;

	uint index = getIndex(u, v);

	uint height;
	if(u >= NOISE_WIDTH || v >= NOISE_HEIGHT)
	{
		skipVert = true;
		height = 0.0f;
	}
	else
	{
		height = noiseMap[index] * HEIGHT_MODIFIER - 0.5 * HEIGHT_MODIFIER;
	}

	HEIGHTS[gtid.y][gtid.x] = height;

	GroupMemoryBarrierWithGroupSync();
	
	//skip to write vertex
	if(!skipVert)
	{
		float3 diff[4];

		uint bits = 0;

		float hCenter = HEIGHTS[gtid.y][gtid.x];

		if(u < (NOISE_WIDTH - 1) && gtid.x < (THREADS_X - 1))
		{
			diff[0] = float3(1, HEIGHTS[gtid.y][gtid.x + 1] - hCenter, 0);
			bits |= 1;
		}
		if(v < (NOISE_HEIGHT - 1) && gtid.y < (THREADS_Y - 1))
		{
			diff[1] = float3(0, HEIGHTS[gtid.y + 1][gtid.x] - hCenter, 1);
			bits |= 2;
		}
		if(u != 0)
		{
			diff[2] = float3(-1, HEIGHTS[gtid.y][gtid.x - 1] - hCenter, 0);
			bits |= 4;
		}
		if(v != 0)
		{
			diff[3] = float3(0, HEIGHTS[gtid.y - 1][gtid.x] - hCenter, -1);
			bits |= 8;
		}

		vertOut[index] = float3(u, hCenter, v) - float3(0.5 * NOISE_WIDTH, 0.0, 0.5 * NOISE_HEIGHT);

		float3 accumNorm = float3(0,0,0);

		[unroll]
		for(uint i = 0; i < 4; ++i)
		{
			uint nextIndex = (i == 3) ? 0 : (i + 1);
			if((bits & (1U << i)) && (bits & (1U << nextIndex)))
			{
				float3 v1 = diff[i];
				float3 v2 = diff[nextIndex];

				accumNorm += normalize(cross(v2, v1));
			}
		}

		normOut[index] = normalize(accumNorm);
	} 
}

[numthreads(THREADS_X, THREADS_Y, 1)]
void genTerrainIndex_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint squareIndex = gtid.x + gtid.y * THREADS_X + THREADS_X * THREADS_Y * groupID.x + groupID.y * (NOISE_WIDTH - 1) * THREADS_Y;

	uint baseU = groupID.x * (THREADS_X) + gtid.x;
	uint baseV = groupID.y * (THREADS_Y) + gtid.y;

	uint index = getIndex(baseU, baseV);
	uint indexBottomRight = getIndex(baseU + 1, baseV + 1);
	uint indexBottomLeft = getIndex(baseU, baseV + 1);
	uint indexTopRight = getIndex(baseU + 1, baseV);

	indexOut[(squareIndex) * 2 + 0] = uint3(index, indexTopRight, indexBottomLeft);
	indexOut[(squareIndex) * 2 + 1] = uint3(indexTopRight, indexBottomLeft, indexBottomRight);
}
