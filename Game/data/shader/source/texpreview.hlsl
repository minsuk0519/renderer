#include "include\packing.hlsli"

RWTexture2D<float4> dst : register(u0);

cbuffer cb_texPreview : register(b0)
{
	uint cb_mode;
	uint cb_mip;
	uint cb_srcWidth;
	uint cb_srcHeight;
	uint cb_dstWidth;
	uint cb_dstHeight;
};

bool texPreviewSrcCoord(uint3 tid, out int3 coord)
{
	coord = int3(0, 0, 0);
	if (tid.x >= cb_dstWidth || tid.y >= cb_dstHeight)
	{
		return false;
	}
	uint sx = (tid.x * cb_srcWidth) / max(cb_dstWidth, 1u);
	uint sy = (tid.y * cb_srcHeight) / max(cb_dstHeight, 1u);
	coord = int3((int)min(sx, cb_srcWidth - 1), (int)min(sy, cb_srcHeight - 1), (int)cb_mip);
	return true;
}

float3 texPreviewIDColor(uint id)
{
	uint h = id * 2654435761u;
	return float3(((h >> 16) & 0xFF) / 255.0, ((h >> 8) & 0xFF) / 255.0, (h & 0xFF) / 255.0);
}

Texture2D<float4> srcFloat : register(t0);

[numthreads(8, 8, 1)]
void texPreviewFloat_cs(uint3 tid : SV_DispatchThreadID)
{
	int3 coord;
	if (!texPreviewSrcCoord(tid, coord))
	{
		return;
	}

	float3 rgb = saturate(srcFloat.Load(coord).rgb);
	if (cb_mode == 4)
	{
		rgb = srcFloat.Load(coord).rrr;
	}

	dst[tid.xy] = float4(rgb, 1.0);
}

Texture2D<uint> srcUint : register(t0);

[numthreads(8, 8, 1)]
void texPreviewUint_cs(uint3 tid : SV_DispatchThreadID)
{
	int3 coord;
	if (!texPreviewSrcCoord(tid, coord))
	{
		return;
	}

	uint v = srcUint.Load(coord);
	float3 rgb = float3(0.0, 0.0, 0.0);

	if (cb_mode == 0)
	{
		// Raw bits
		rgb = float3(((v >> 16) & 0xFF) / 255.0, ((v >> 8) & 0xFF) / 255.0, (v & 0xFF) / 255.0);
	}
	else if (cb_mode == 1)
	{
		// Octahedral normal
		rgb = decodeOct(v) * 0.5 + 0.5;
	}
	else if (cb_mode == 2)
	{
		// Object ID
		rgb = texPreviewIDColor(packedIDToObjID(v));
	}
	else if (cb_mode == 3)
	{
		// visID cluster
		uint slot, tri;
		decodeVisID(v, slot, tri);
		rgb = texPreviewIDColor(slot);
	}

	dst[tid.xy] = float4(rgb, 1.0);
}
