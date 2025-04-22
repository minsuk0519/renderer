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
#if ENGINE_DEBUG_DEBUGCAM
		CAMTYPE_DEBUG = 1,
#endif // #if ENGINE_DEBUG_DEBUGCAM
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
	void updateView();

	DirectX::XMVECTOR* getFrustum();

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

#if ENGINE_DEBUG_DEBUGCAM
	void toggleDebugMode();
#endif // #if ENGINE_DEBUG_DEBUGCAM
private:
#if ENGINE_DEBUG_DEBUGCAM
	cam::CAMTYPE type = cam::CAMTYPE_DEBUG;
#else #if ENGINE_DEBUG_DEBUGCAM
	cam::CAMTYPE type = cam::CAMTYPE_MAIN;
#endif // #else #if ENGINE_DEBUG_DEBUGCAM

#if ENGINE_DEBUG_DEBUGCAM
	bool debugMode = false;
#endif // #if ENGINE_DEBUG_DEBUGCAM

	void move(float x, float y, int mousex, int mousey, float dt);

	transform* transformPtr = nullptr;

	DirectX::XMVECTOR frustum[6];
};