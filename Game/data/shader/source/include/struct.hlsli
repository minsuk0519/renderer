struct projection
{
    float4x4 projectionMat;
    float4x4 viewMat;
    float3 camPos;
	float farPlane;
};

struct object
{
	float4x4 objectMat;
    float3 albedo;
    float metal;
    float roughness;
};

struct cmdBuf
{
    uint cbv;

    uint VertexCountPerInstance;
    uint InstanceCount;
    uint StartVertexLocation;
    uint StartInstanceLocation;
};

struct cmdBufCluster
{
    uint threadCountX;
    uint threadCountY;
    uint threadCountZ;
};

//meshInfos
struct meshInfo
{
    uint lodOffset;
    uint lodNum;
    uint vertexOffset;
    uint flags;
};

#define MESHINFO_STRUCT_SIZE    4 * 4

void getMeshInfoFromBuffer(ByteAddressBuffer buffer, uint index, out meshInfo result)
{
    uint4 data = buffer.Load4(MESHINFO_STRUCT_SIZE * index);

    result.lodOffset = data.x;
    result.lodNum = data.y;
    result.vertexOffset = data.z;
    result.flags = data.w;
}

//lodInfos
struct lodInfo
{
    uint clusterCount;
    uint clusterOffset;
    uint indexSize;
};
//clusterInfos
struct clusterInfo
{
    uint indexSize;
    uint indexOffset;
};


#define GPU_MESH_INFO_FLAGS_TERRAIN		(1U << 0)
#define GPU_MESH_INFO_FLAGS_NONORM		(1U << 1)