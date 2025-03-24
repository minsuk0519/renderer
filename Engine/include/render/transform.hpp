#pragma once

#include <DirectXMath.h>

struct transform
{
private:
	DirectX::XMVECTOR position;
	DirectX::XMVECTOR scale = DirectX::XMVECTOR{ 1, 1, 1 };

	//rotation with euler angle
	DirectX::XMVECTOR rotation;

	DirectX::XMMATRIX mat;

	DirectX::XMVECTOR up = DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f };
	DirectX::XMVECTOR right = DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f };

	DirectX::XMMATRIX worldMatrix;

public:
	float* getMatPointer();
	DirectX::XMMATRIX getMat();

	void movePosition(DirectX::XMVECTOR offset);
	void applyScale(DirectX::XMVECTOR offset);

	DirectX::XMVECTOR getPosition() const;
	DirectX::XMVECTOR getScale() const;
	DirectX::XMVECTOR getRotation() const;

	float* getPosPointer();
	float* getScalePointer();
	float* getRotationPointer();

	DirectX::XMVECTOR getQuaternion() const;

	DirectX::XMVECTOR getUP() const;
	DirectX::XMVECTOR getRIGHT() const;

	void setPosition(DirectX::XMVECTOR pos);
	void setScale(DirectX::XMVECTOR size);
	void setRotation(DirectX::XMVECTOR rot);

};