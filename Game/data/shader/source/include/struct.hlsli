#include "helper.hlsli"

#define BUFFER_ELEMENT_SIZE 4

struct projection
{
    float4x4 viewProj;
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
#define MESHINFO_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * 4

//lodInfos
struct lodInfo
{
    uint clusterCount;
    uint clusterOffset;
    uint indexSize;
};
#define LODINFO_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * 3

//clusterInfos
struct clusterInfo
{
    uint indexSize;
    uint indexOffset;
};
#define CLUSTERINFO_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * 2

//clusterbounds
struct clusterBound
{
    float3 sphereCenter;
    float sphereRadius;
    float3 aabbCenter;
    float3 aabbhExtent;
};
#define CLUSTERBOUND_STRUCT_ELEMENT_SIZE    10
#define CLUSTERBOUND_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * CLUSTERBOUND_STRUCT_ELEMENT_SIZE

//viewinfo
struct viewInfo
{
    float3 translate;
    float3 scale;
    float4 rotation;
};
#define VIEWINFO_STRUCT_ELEMENT_SIZE    10
#define VIEWINFO_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * VIEWINFO_STRUCT_ELEMENT_SIZE

//uvb
#define VERTEX_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * 3

//uib
#define INDEX_STRUCT_SIZE    BUFFER_ELEMENT_SIZE * 3

#define GPU_MESH_INFO_FLAGS_TERRAIN		(1U << 0)
#define GPU_MESH_INFO_FLAGS_NONORM		(1U << 1)