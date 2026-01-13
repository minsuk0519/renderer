#include "globalbuffers.hlsli"

void getVertexIndex(uint index, out uint ID)
{
    ID = UIB.Load(index * BUFFER_ELEMENT_SIZE);
}

void getVertex(uint index, out float3 vertex)
{
    vertex = asfloat(UVB.Load3(index * VERTEX_STRUCT_SIZE));
}

void getNormal(uint index, out float3 normal)
{
    normal = asfloat(UVB.Load3((index + vertexMax) * VERTEX_STRUCT_SIZE));
}

void getViewInfoFromBuffer(ByteAddressBuffer buffer, uint index, out viewInfo result)
{
    float3 data1 = asfloat(buffer.Load3((VIEWINFO_STRUCT_ELEMENT_SIZE * index + 0) * BUFFER_ELEMENT_SIZE));
    float3 data2 = asfloat(buffer.Load3((VIEWINFO_STRUCT_ELEMENT_SIZE * index + 3) * BUFFER_ELEMENT_SIZE));
    float4 data3 = asfloat(buffer.Load4((VIEWINFO_STRUCT_ELEMENT_SIZE * index + 6) * BUFFER_ELEMENT_SIZE));

    result.translate = data1;
    result.scale = data2;
    result.rotation = data3;
}

void getViewInfo(uint index, out viewInfo result)
{
    float3 data1 = asfloat(viewInfos.Load3((VIEWINFO_STRUCT_ELEMENT_SIZE * index + 0) * BUFFER_ELEMENT_SIZE));
    float3 data2 = asfloat(viewInfos.Load3((VIEWINFO_STRUCT_ELEMENT_SIZE * index + 3) * BUFFER_ELEMENT_SIZE));
    float4 data3 = asfloat(viewInfos.Load4((VIEWINFO_STRUCT_ELEMENT_SIZE * index + 6) * BUFFER_ELEMENT_SIZE));

    result.translate = data1;
    result.scale = data2;
    result.rotation = data3;
}

void getMeshInfoFromBuffer(ByteAddressBuffer buffer, uint index, out meshInfo result)
{
    uint4 data = buffer.Load4(MESHINFO_STRUCT_SIZE * index);

    result.lodOffset = data.x;
    result.lodNum = data.y;
    result.vertexOffset = data.z;
    result.flags = data.w;
}

void getMeshInfo(uint index, out meshInfo result)
{
    uint4 data = meshInfos.Load4(MESHINFO_STRUCT_SIZE * index);

    result.lodOffset = data.x;
    result.lodNum = data.y;
    result.vertexOffset = data.z;
    result.flags = data.w;
}

void getLODInfoFromBuffer(ByteAddressBuffer buffer, uint index, out lodInfo result)
{
    uint3 data = buffer.Load3(LODINFO_STRUCT_SIZE * index);

    result.clusterCount = data.x;
    result.clusterOffset = data.y;
    result.indexSize = data.z;
}

void getLODInfo(uint index, out lodInfo result)
{
    uint3 data = lodInfos.Load3(LODINFO_STRUCT_SIZE * index);

    result.clusterCount = data.x;
    result.clusterOffset = data.y;
    result.indexSize = data.z;
}

void getClusterInfoFromBuffer(ByteAddressBuffer buffer, uint index, out clusterInfo result)
{
    uint3 data = buffer.Load3(CLUSTERINFO_STRUCT_SIZE * index);

    result.indexSize = data.x;
    result.indexOffset = data.y;
}

void getClusterInfo(uint index, out clusterInfo result)
{
    uint3 data = clusterInfos.Load3(CLUSTERINFO_STRUCT_SIZE * index);

    result.indexSize = data.x;
    result.indexOffset = data.y;
}

void getClusterBoundFromBuffer(ByteAddressBuffer buffer, uint index, out clusterBound result)
{
    float4 data1 = asfloat(buffer.Load4((CLUSTERBOUND_STRUCT_ELEMENT_SIZE * index + 0) * BUFFER_ELEMENT_SIZE));
    float3 data2 = asfloat(buffer.Load3((CLUSTERBOUND_STRUCT_ELEMENT_SIZE * index + 4) * BUFFER_ELEMENT_SIZE));
    float3 data3 = asfloat(buffer.Load3((CLUSTERBOUND_STRUCT_ELEMENT_SIZE * index + 7) * BUFFER_ELEMENT_SIZE));

    result.sphereCenter = data1.xyz;
    result.sphereRadius = data1.w;
    result.aabbCenter = data2;
    result.aabbhExtent = data3;
}

void getClusterBound(uint index, out clusterBound result)
{
    float4 data1 = asfloat(clusterBounds.Load4((CLUSTERBOUND_STRUCT_ELEMENT_SIZE * index + 0) * BUFFER_ELEMENT_SIZE));
    float3 data2 = asfloat(clusterBounds.Load3((CLUSTERBOUND_STRUCT_ELEMENT_SIZE * index + 4) * BUFFER_ELEMENT_SIZE));
    float3 data3 = asfloat(clusterBounds.Load3((CLUSTERBOUND_STRUCT_ELEMENT_SIZE * index + 7) * BUFFER_ELEMENT_SIZE));

    result.sphereCenter = data1.xyz;
    result.sphereRadius = data1.w;
    result.aabbCenter = data2;
    result.aabbhExtent = data3;
}