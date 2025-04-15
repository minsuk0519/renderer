//(w x y z)
float3 quatRotate(float4 q, float3 v)
{
    float3 result;

    result = 2.0f * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2.0f * q.w * cross(q.xyz, v);

    return result;
}

void getAxisVecFromQuat(float4 q, out float3 right, out float3 up, out float3 forward)
{
    float coefficient = (q.w * q.w - dot(q.xyz, q.xyz));

    right = 2.0f * q.x * q.xyz + coefficient * float3(1,0,0) + 2.0f * q.w * float3(0.0f, q.z, -q.y);
    up = 2.0f * q.y * q.xyz + coefficient * float3(0,1,0) + 2.0f * q.w * float3(-q.z, 0.0f, q.x);
    forward = 2.0f * q.z * q.xyz + coefficient * float3(0,0,1) + 2.0f * q.w * float3(q.y, -q.x, 0.0f);
}

float3 transformToWorld(float3 scale, float4 quaternion, float3 translate, float3 position)
{
    float3 scaledPos;
        scaledPos.x = position.x * scale.x;
        scaledPos.y = position.y * scale.y;
        scaledPos.z = position.z * scale.z;
    float3 worldPos = quatRotate(quaternion, scaledPos);
    worldPos += translate;
    return worldPos;
}