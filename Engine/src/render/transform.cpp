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
