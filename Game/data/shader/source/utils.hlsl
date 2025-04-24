RWStructuredBuffer<uint> dstBuffer : register(u0);

StructuredBuffer<uint> srcBuffer : register(t0);

//all shader that need to utilize wave intrinsic have to be 32 unless using tgsm
#define THREADS_NUM 64

cbuffer cb_utilBuf : register(b0)
{
	uint sumCount;
	uint elementStride;
	uint elementOffset;
	uint pad;
}

//deprecated
[numthreads(THREADS_NUM, 1, 1)]
void sumOffset_cs( uint3 groupID : SV_GroupID, uint3 gtid : SV_GroupThreadID, uint threadID : SV_GroupIndex )
{
	if(gtid.x >= sumCount)
	{
		return;
	}

	uint offset = (gtid.x == 0) ? 0 : srcBuffer[(gtid.x * 64 - 1) * elementStride + elementOffset];

	//uint sum = WavePrefixSum(offset);

	dstBuffer[gtid.x] = offset;//sum;
}