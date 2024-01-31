#pragma once

#include <system\defines.hpp>
#include <render\descriptorheap.hpp>

#include <d3dx12.h>
#include <wrl.h>

#include <DirectXMath.h>

struct transform;

struct CD3DX12_VIEWPORT;
struct CD3DX12_RECT;
struct constantbuffer;

namespace cam
{
	struct viewport
	{
		float topLeftX;
		float topLeftY;
		float width;
		float height;
	};

	struct scissorRect
	{
		long left;
		long right;
		long top;
		long bottom;
	};

	enum VIEWPORT_TYPE
	{
		VIEWPORT_MINI,
		VIEWPORT_FULL,
	};

	enum CAMTYPE
	{
		CAMTYPE_MAIN = 0,
		CAMTYPE_DEBUG = 1,
		CAMTYPE_INVALID = -1,
	};
};

class camera
{
public:
	bool init();
	void update(float dt);
	void close();

	//the main camera have to be unique entity
	void setCamAsMain();

	void draw(uint psoIndex, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);
	void preDraw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList);

	void changeViewport(const cam::VIEWPORT_TYPE type);
	
	cam::viewport screenViewport;
	cam::scissorRect scissor = {};

	constantbuffer* projectionBuffer;
	descriptor desc;

	//for drawing frustum
	constantbuffer* objectBuffer;
	descriptor objectdesc;

	cam::VIEWPORT_TYPE viewportType = cam::VIEWPORT_FULL;

	transform* getTransform() const;

	DirectX::XMMATRIX getMat() const;

	void toggleDebugMode();
private:
	cam::CAMTYPE type = cam::CAMTYPE_DEBUG;

	bool debugMode = false;

	void move(float x, float y, int mousex, int mousey, float dt);

	transform* transformPtr = nullptr;
};