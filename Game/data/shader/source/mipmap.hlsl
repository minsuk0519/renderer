#include "include\helper.hlsli"

Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);
cbuffer cb_texSize : register(b0)
{
	uint srcWidth;
	uint srcHeight;
	uint dstWidth;
	uint dstHeight;
};

[numthreads(8, 8, 1)]
void genHZB_cs(uint3 dtid : SV_DispatchThreadID)
{
	if (dtid.x >= dstWidth || dtid.y >= dstHeight)
	{
		return;
	}

	uint x0 = min(dtid.x * 2, srcWidth - 1);
	uint x1 = (dtid.x == dstWidth - 1) ? (srcWidth - 1) : min(dtid.x * 2 + 1, srcWidth - 1);
	uint y0 = min(dtid.y * 2, srcHeight - 1);
	uint y1 = (dtid.y == dstHeight - 1) ? (srcHeight - 1) : min(dtid.y * 2 + 1, srcHeight - 1);

	float depth = src.Load(int3(x0, y0, 0));
	for (uint sy = y0; sy <= y1; ++sy)
	{
		for (uint sx = x0; sx <= x1; ++sx)
		{
			depth = min(depth, src.Load(int3(sx, sy, 0)));
		}
	}

	dst[dtid.xy] = depth;
}
