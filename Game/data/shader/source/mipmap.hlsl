#include "include\helper.hlsli"

Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);

SamplerState maxSampler 		: register(s0);

cbuffer cb_texSize : register(b0)
{
	uint texWidth;
	uint texHeight;
    uint mipLevelPass;
};

groupshared float gsMaxDepth[64];

[numthreads(8, 8, 1)]
void genHZB_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
    uint tileSize = 8 * 8;

	uint2 uv = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);
    uint ldsID = tiledSwizzling(gtid.xy, mipLevelPass - 1);

    float2 centerUV = float2((uv.x + 0.5) / (texWidth - 1), (uv.y + 0.5) / (texHeight - 1));

    float4 d = src.GatherRed(maxSampler, centerUV, 0);
    
    float maxDepth = max(max(d[0], d[1]), max(d[2], d[3]));

    //mip 1
    dst[uv.xy] = maxDepth;
    tileSize = tileSize >> 2;

    gsMaxDepth[ldsID] = maxDepth;

    GroupMemoryBarrierWithGroupSync();
    //mip 2 - 4
    for(uint i = 1; i < mipLevelPass; ++i)
    {
        if(ldsID < tileSize)
        {
            for(uint j = 0; j < 4; ++j)
            {
                d[j] = gsMaxDepth[ldsID + j * tileSize];
            }

            maxDepth = max(max(d[0], d[1]), max(d[2], d[3]));
            gsMaxDepth[ldsID] = maxDepth;
            tileSize = tileSize >> 2;
        }

        if(!tileSize) break;
    }    
}