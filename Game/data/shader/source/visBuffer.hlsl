#include "include\common.hlsli"
#include "include\packing.hlsli"

Texture2D<uint> visIDGbuffer : register(t0);
Texture2D<float> sceneDepth : register(t1);
ByteAddressBuffer clusterArgs : register(t2);
RWByteAddressBuffer materialPixelCounts : register(u0);

struct visBufferSample
{
    uint clusterSlot;
    uint localTri;
    uint packedID;
    uint objID;
    uint meshIndex;
    uint lod;
};

bool visBuffer_traverse(uint2 pixel, out visBufferSample result)
{
    result = (visBufferSample)0;

    if (pixel.x >= screenWidth || pixel.y >= screenHeight)
    {
        return false;
    }

    float depth = sceneDepth[pixel];
    if (depth <= 0.0f)
    {
        return false;
    }

    uint visID = visIDGbuffer[pixel];
    uint clusterSlot, localTri;
    decodeVisID(visID, clusterSlot, localTri);

    result.packedID = clusterArgs.Load((clusterSlot * 3 + 2) * 4);

    uint objID = INVALID_ID;
    decodePackedID(result.packedID, objID, result.meshIndex, result.lod);

    result.clusterSlot = clusterSlot;
    result.localTri = localTri;

    return objID < MAX_MATERIAL_NUM;
}

[numthreads(64, 1, 1)]
void visBuffer_initMaterialCount_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint threadIndex = gtid.x;
    uint slotsToInitialize = MAX_MATERIAL_NUM;

    for (uint i = threadIndex; i < slotsToInitialize; i += 64)
    {
        materialPixelCounts.Store(i * 4, 0);
    }
}

[numthreads(8, 8, 1)]
void visBuffer_materialCount_cs(uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID)
{
    uint2 quad = uint2(groupID.x * 8 + gtid.x, groupID.y * 8 + gtid.y);

    uint matID[4];

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        uint2 p = quad * 2 + uint2(i & 1, i >> 1);

        visBufferSample s;
        if(visBuffer_traverse(p, s))
        {
            matID[i] = s.objID;
        }
        else
        {
            matID[i] = INVALID_ID;
        }
    }

    uint iterCap = 4 * WaveGetLaneCount();
    for (uint iter = 0; iter < iterCap; ++iter)
    {
        uint laneMin = min(min(matID[0], matID[1]), min(matID[2], matID[3]));
        uint waveMin = WaveActiveMin(laneMin);

        if (waveMin == INVALID_ID)
        {
            break;
        }

        uint laneHits = 0;
        [unroll]
        for (uint j = 0; j < 4; ++j)
        {
            if (matID[j] == waveMin)
            {
                ++laneHits;
                matID[j] = INVALID_ID;
            }
        }

        uint waveHits = WaveActiveSum(laneHits);

        if (WaveIsFirstLane())
        {
            uint prev;
            materialPixelCounts.InterlockedAdd(waveMin * 4, waveHits, prev);
        }
    }
}
