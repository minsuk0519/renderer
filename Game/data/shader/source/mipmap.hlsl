#include "include\helper.hlsli"

Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);
RWTexture2D<float> dst1 : register(u1);
RWTexture2D<float> dst2 : register(u2);
RWTexture2D<float> dst3 : register(u3);

SamplerState maxSampler 		: register(s0);

cbuffer cb_texSize : register(b0)
{
	uint texWidth;
	uint texHeight;
    uint mipLevelPass;
};

groupshared float gsDepth[8][8];

[numthreads(8, 8, 1)]
void genHZB_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	uint2 uv = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);

    float2 centerUV = float2((uv.x + 0.5) / texWidth, (uv.y + 0.5) / texHeight);

    float4 d = src.GatherRed(maxSampler, centerUV, 0);

    float depth = min(min(d[0], d[1]), min(d[2], d[3]));

    //mip m (level 0)
    dst[uv.xy] = depth;

    gsDepth[gtid.x][gtid.y] = depth;

    GroupMemoryBarrierWithGroupSync();

    //mip m+1 (level 1)
    if (mipLevelPass > 1)
    {
        if (gtid.x < 4 && gtid.y < 4)
        {
            float d0 = gsDepth[gtid.x * 2 + 0][gtid.y * 2 + 0];
            float d1 = gsDepth[gtid.x * 2 + 1][gtid.y * 2 + 0];
            float d2 = gsDepth[gtid.x * 2 + 0][gtid.y * 2 + 1];
            float d3 = gsDepth[gtid.x * 2 + 1][gtid.y * 2 + 1];

            depth = min(min(d0, d1), min(d2, d3));
            gsDepth[gtid.x][gtid.y] = depth;

            uint2 dstUV1 = uint2(groupID.x * 4 + gtid.x, groupID.y * 4 + gtid.y);
            dst1[dstUV1] = depth;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    //mip m+2 (level 2)
    if (mipLevelPass > 2)
    {
        if (gtid.x < 2 && gtid.y < 2)
        {
            float d0 = gsDepth[gtid.x * 2 + 0][gtid.y * 2 + 0];
            float d1 = gsDepth[gtid.x * 2 + 1][gtid.y * 2 + 0];
            float d2 = gsDepth[gtid.x * 2 + 0][gtid.y * 2 + 1];
            float d3 = gsDepth[gtid.x * 2 + 1][gtid.y * 2 + 1];

            depth = min(min(d0, d1), min(d2, d3));
            gsDepth[gtid.x][gtid.y] = depth;

            uint2 dstUV2 = uint2(groupID.x * 2 + gtid.x, groupID.y * 2 + gtid.y);
            dst2[dstUV2] = depth;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    //mip m+3 (level 3)
    if (mipLevelPass > 3)
    {
        if (gtid.x < 1 && gtid.y < 1)
        {
            float d0 = gsDepth[0][0];
            float d1 = gsDepth[1][0];
            float d2 = gsDepth[0][1];
            float d3 = gsDepth[1][1];

            depth = min(min(d0, d1), min(d2, d3));

            uint2 dstUV3 = uint2(groupID.x + 0, groupID.y + 0);
            dst3[dstUV3] = depth;
        }
    }
}
