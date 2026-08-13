#include "render\transform.hpp"

namespace AXIS
{
	DirectX::XMVECTOR UP_AXIS = DirectX::XMVECTOR{ 0.0, 1.0f, 0.0f };
	DirectX::XMVECTOR RIGHT_AXIS = DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f };
	DirectX::XMVECTOR FORWARD_AXIS = DirectX::XMVECTOR{ 0.0, 0.0f, -1.0f };
}

float* transform::getMatPointer()
{
	DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(position);
	DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYawFromVector(rotation);
	DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(scale);
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	return &worldMatrix.r[0].m128_f32[0];
}

DirectX::XMMATRIX transform::getMat()
{
	DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslationFromVector(position);
	DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYawFromVector(rotation);
	DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScalingFromVector(scale);
	worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;

	return worldMatrix;
}

void transform::movePosition(DirectX::XMVECTOR offset)
{
	position = DirectX::XMVectorAdd(position, offset);
}

void transform::applyScale(DirectX::XMVECTOR offset)
{
	scale = DirectX::XMVectorMultiply(scale, offset);
}

DirectX::XMVECTOR transform::getPosition() const
{
	return position;
}

DirectX::XMVECTOR transform::getScale() const
{
	return scale;
}

DirectX::XMVECTOR transform::getRotation() const
{
	return rotation;
}

float* transform::getPosPointer()
{
	return &position.m128_f32[0];
}

float* transform::getScalePointer()
{
	return &scale.m128_f32[0];
}

float* transform::getRotationPointer()
{
	return &rotation.m128_f32[0];
}

DirectX::XMVECTOR transform::getQuaternion() const
{
	return DirectX::XMQuaternionRotationRollPitchYawFromVector(rotation);
}

DirectX::XMVECTOR transform::getUP() const
{
	return up;
}

DirectX::XMVECTOR transform::getRIGHT() const
{
	return right;
}

void transform::setPosition(DirectX::XMVECTOR pos)
{
	position = pos;
}

void transform::setScale(DirectX::XMVECTOR size)
{
	scale = size;
}

void transform::setRotation(DirectX::XMVECTOR rot)
{
	rotation = rot;

	up = DirectX::XMVector3Rotate(AXIS::UP_AXIS, rot);
	right = DirectX::XMVector3Rotate(AXIS::RIGHT_AXIS, rot);
}

//copy and paste from xmmatrix library and modify some values so that NDC Z range is 1 to 0
DirectX::XMMATRIX buildProjMatrixInternal(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
{
    assert(NearZ > 0.f && FarZ > 0.f);
    assert(!DirectX::XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    assert(!DirectX::XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));
    assert(!DirectX::XMScalarNearEqual(FarZ, NearZ, 0.00001f));

#if defined(_XM_NO_INTRINSICS_)

    float    SinFov;
    float    CosFov;
    DirectX::XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;
    float fRange = FarZ / (NearZ - FarZ);

    DirectX::XMMATRIX M;
    M.m[0][0] = Width;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = Height;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = fRange;
    M.m[2][3] = -1.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = fRange * NearZ;
    M.m[3][3] = 0.0f;
    return M;

#elif defined(_XM_ARM_NEON_INTRINSICS_)
    float    SinFov;
    float    CosFov;
    DirectX::XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);
    float fRange = FarZ / (NearZ - FarZ);
    float Height = CosFov / SinFov;
    float Width = Height / AspectRatio;
    const float32x4_t Zero = vdupq_n_f32(0);

    DirectX::XMMATRIX M;
    M.r[0] = vsetq_lane_f32(Width, Zero, 0);
    M.r[1] = vsetq_lane_f32(Height, Zero, 1);
    M.r[2] = vsetq_lane_f32(fRange, g_XMNegIdentityR3.v, 2);
    M.r[3] = vsetq_lane_f32(fRange * NearZ, Zero, 2);
    return M;
#elif defined(_XM_SSE_INTRINSICS_)
    float    SinFov;
    float    CosFov;
    DirectX::XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);
    float fRange = NearZ / (FarZ - NearZ);
    // Note: This is recorded on the stack
    float Height = CosFov / SinFov;
    DirectX::XMVECTOR rMem = {
        Height / AspectRatio,
        Height,
        fRange,
        fRange * FarZ
    };
    // Copy from memory to SSE register
    DirectX::XMVECTOR vValues = rMem;
    DirectX::XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp, vValues);
    // Height / AspectRatio,0,0,0
    DirectX::XMMATRIX M;
    M.r[0] = vTemp;
    // 0,Height,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp, DirectX::g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=-fRange * NearZ,0,-1.0f
    vTemp = _mm_setzero_ps();
    vValues = _mm_shuffle_ps(vValues, DirectX::g_XMNegIdentityR3, _MM_SHUFFLE(3, 2, 3, 2));
    // 0,0,fRange,-1.0f
    vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(3, 0, 0, 0));
    M.r[2] = vTemp;
    // 0,0,fRange * NearZ,0.0f
    vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(2, 1, 0, 0));
    M.r[3] = vTemp;
    return M;
#endif
}

DirectX::XMMATRIX transform::buildViewProjMat(float fov, float aspectRatio, float nearZ, float farZ) const
{
    DirectX::XMVECTOR up = getUP();
    DirectX::XMVECTOR right = getRIGHT();

    DirectX::XMVECTOR forward = DirectX::XMVector3Cross(up, right);

    DirectX::XMVECTOR pos = getPosition();

    DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(pos, forward, up);

    DirectX::XMMATRIX proj = buildProjMatrixInternal(fov, aspectRatio, nearZ, farZ);

    return DirectX::XMMatrixMultiply(view, proj);
}
