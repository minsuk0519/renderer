#include "include\common.hlsli"
#include "include\packing.hlsli"
#include "include\math.hlsli"

ByteAddressBuffer clusterArgs : register(t0);

struct PSInput
{
	float4 position : SV_POSITION;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;

    uint4 output : OUTPUT;
};

struct PSOutput
{
    float4 position     : SV_TARGET0;
    uint normTex      	: SV_TARGET1;
    uint debugID      	: SV_TARGET2;
};

cbuffer cb_objectIdentification : register(b2)
{
    uint objectID;
}

PSInput gbufferIndirect_vs(uint vertexID : SV_VertexID, uint clusterID : SV_InstanceID)
{
    PSInput result;

    uint indexOffset = clusterArgs.Load((clusterID * 3 + 0) * 4);
    uint indexSize = clusterArgs.Load((clusterID * 3 + 1) * 4);
    uint packedID = clusterArgs.Load((clusterID * 3 + 2) * 4);

    uint objectInfo = packedID & ((1 << 16) - 1);
    uint meshIndex = objectInfo >> 3;
    uint objID = packedID >> 16;

    meshInfo mesh;
    getMeshInfo(meshIndex, mesh);

    uint vertexOffset = mesh.vertexOffset;

    uint vertexIndex;
    getVertexIndex(indexOffset + vertexID, vertexIndex);

    float3 position;
    getVertex(vertexOffset + vertexIndex, position);
    float3 normal;
    getNormal(vertexOffset + vertexIndex + vertexMax, normal);

    if(vertexID < indexSize)
    {
        viewInfo view;
        getViewInfo(objID, view);
        float3 translate = view.translate;
        float3 scale = view.scale;
        float4 rotation = view.rotation;
        float3 worldPos = transformToWorld(scale, rotation, translate, position);
        result.worldPos = worldPos;
        result.position = mul(proj.viewProj, float4(worldPos, 1.0f));

        float3 scaledNorm;
        scaledNorm.x = normal.x * scale.x;
        scaledNorm.y = normal.y * scale.y;
        scaledNorm.z = normal.z * scale.z;
        result.normal = normalize(quatRotate(rotation, scaledNorm));

        //result.output = float4(clusterID,meshIndex,objID,vertexIndex);
        result.output = uint4(vertexOffset, vertexIndex, indexOffset, vertexID);
    }
    return result;
}

PSInput gbuffer_vs(float3 position : VPOSITION, float3 normal : VNORMAL)
{
    PSInput result;

    float4 worldPos = mul(obj.objectMat, float4(position, 1.0));
    result.worldPos = worldPos.xyz;
    result.position = mul(proj.viewProj, worldPos);

    result.output.x = 0;
    result.output.y = 0;
    result.output.z = 0;
    result.output.w = 0;

    result.normal = normalize(mul((float3x3)(obj.objectMat), normal));

    return result;
}

PSOutput gbuffer_ps(PSInput input)
{
    float3 position = input.worldPos;
	float3 normal = normalize(input.normal);

    PSOutput result;

    result.position = float4(position, 1.0f);
    result.normTex = encodeOct(normal);
    result.debugID = input.output.x + 1;//input.output.x;
    
    return result;
}
