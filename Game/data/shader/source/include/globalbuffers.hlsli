#include "include\struct.hlsli"

#define DEBUG 1

#if DEBUG
RWByteAddressBuffer outDebugData : register(u63);
#endif // #if DEBUG

///srv 63 - 48 will be occupied for global buffers
ByteAddressBuffer UVB : register(t63);
ByteAddressBuffer UIB : register(t62);

ByteAddressBuffer meshInfos : register(t61);
ByteAddressBuffer lodInfos : register(t60);
ByteAddressBuffer clusterInfos : register(t59);
ByteAddressBuffer clusterBounds : register(t58);
ByteAddressBuffer viewInfos : register(t57);
ByteAddressBuffer materialInfos : register(t56);

cbuffer cb_cmdBuf : register(b63)
{
    //objID, meshIndex
    uint4 cbObj[MAX_OBJ_NUM / 4];
    uint objCount;
    uint postObjOffset;
    uint postObjCount;
    uint pad;
}
//objID, meshIndex, lod
//16 : 13 : 3
static uint packedObj[MAX_OBJ_NUM] = (uint[MAX_OBJ_NUM])cbObj;

cbuffer projectionBuffer : register(b62)
{
    projection proj;
};

cbuffer objectBuffer : register(b61)
{
    object obj;
};

cbuffer objectsBuffer : register(b60)
{
    object objs[MAX_OBJ_NUM];
};

cbuffer cb_window : register(b59)
{
	uint screenWidth;
	uint screenHeight;
};

SamplerState samp 			: register(s0);
