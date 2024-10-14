#include "include\common.hlsli"

RWStructuredBuffer<float3> vertOut : register(u0);
RWStructuredBuffer<float3> normOut : register(u1);
RWStructuredBuffer<uint3> indexOut : register(u2);

StructuredBuffer<float> noiseMap : register(t0);

static const int THREADS_X = 8;
static const int THREADS_Y = 8;

static const int CLUSTER_X = 4;
static const int CLUSTER_Y = 4;

static const int NUMTHREADS = THREADS_X * THREADS_Y;

cbuffer cb_Height : register(b0)
{
	float HEIGHT_MODIFIER = 100.0f;
}

groupshared float HEIGHTS[THREADS_Y + 1][THREADS_X + 1];

[numthreads(THREADS_X, THREADS_Y, 1)]
void terrain_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint index = (groupID.x + groupID.y * (NOISE_WIDTH - 1) / THREADS_X) * NUMTHREADS + gtid.x % CLUSTER_X + ((NUMTHREADS / CLUSTER_X) * (gtid.x / CLUSTER_X)) + CLUSTER_X * (gtid.y % CLUSTER_Y) + ((NUMTHREADS / CLUSTER_X) * (THREADS_X / CLUSTER_X) * (gtid.y / CLUSTER_Y));
	//uint index = ((groupID.y * THREADS_Y + gtid.y) * NOISE_WIDTH + groupID.x * THREADS_X + gtid.x);

	float u = groupID.x * THREADS_X + gtid.x;
	float v = groupID.y * THREADS_Y + gtid.y;

	float height = noiseMap[u + v * NOISE_WIDTH] * HEIGHT_MODIFIER;

	HEIGHTS[gtid.y][gtid.x] = height;

	if(gtid.y == (THREADS_Y - 1) && gtid.x == (THREADS_X - 1)) HEIGHTS[THREADS_Y][THREADS_X] = noiseMap[u + 1 + (v + 1) * NOISE_WIDTH] * HEIGHT_MODIFIER;
	if(gtid.y == (THREADS_Y - 1)) HEIGHTS[THREADS_Y][gtid.x] = noiseMap[u + (v + 1) * NOISE_WIDTH] * HEIGHT_MODIFIER;
	if(gtid.x == (THREADS_X - 1)) HEIGHTS[gtid.y][THREADS_X] = noiseMap[u + 1 + v * NOISE_WIDTH] * HEIGHT_MODIFIER;

	GroupMemoryBarrierWithGroupSync();

	float h1 = HEIGHTS[gtid.y][gtid.x] - 0.5 * HEIGHT_MODIFIER;
	float h2 = HEIGHTS[gtid.y][gtid.x + 1] - 0.5 * HEIGHT_MODIFIER;
	float h3 = HEIGHTS[gtid.y + 1][gtid.x] - 0.5 * HEIGHT_MODIFIER;
	float h4 = HEIGHTS[gtid.y + 1][gtid.x + 1] - 0.5 * HEIGHT_MODIFIER;

	vertOut[index * 4 + 0] = float3(u, h1, v) / (NOISE_WIDTH - 1) - float3(0.5, 0.0, 0.5);
	vertOut[index * 4 + 1] = float3(u + 1, h2, v) / (NOISE_WIDTH - 1) - float3(0.5, 0.0, 0.5);
	vertOut[index * 4 + 2] = float3(u, h3, v + 1) / (NOISE_WIDTH - 1) - float3(0.5, 0.0, 0.5);
	vertOut[index * 4 + 3] = float3(u + 1, h4, v + 1) / (NOISE_WIDTH - 1) - float3(0.5, 0.0, 0.5);

	float3 v1 = float3(1, h2 - h1, 0);
	float3 v2 = float3(0, h3 - h1, 1);

	float3 norm = normalize(cross(v2, v1));

	normOut[index * 4 + 0] = norm;
	normOut[index * 4 + 1] = norm;
	normOut[index * 4 + 2] = norm;
	normOut[index * 4 + 3] = norm;

	indexOut[index * 2 + 0] = uint3(index * 4 + 0, index * 4 + 2, index * 4 + 1);
	indexOut[index * 2 + 1] = uint3(index * 4 + 2, index * 4 + 3, index * 4 + 1);
}