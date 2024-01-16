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