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

//meshInfos
struct meshInfo
{
    uint lodOffset;
    uint lodNum;
    uint vertexOffset;
};
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
