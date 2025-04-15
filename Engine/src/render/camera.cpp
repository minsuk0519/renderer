#include <render\camera.hpp>
#include <render\transform.hpp>
#include <render/buffer.hpp>
#include <render\descriptorheap.hpp>
#include <render\pipelinestate.hpp>
#include <system/window.hpp>
#include <system/input.hpp>
#include <render/commandqueue.hpp>
#include <render/shader_defines.hpp>

#include <DirectXMath.h>
#include <cmath>

constexpr float SPEED = 0.0003f;

constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 100.0f;

constexpr float FOV = 60.0f;

constexpr uint MINI_VIEWPORT_WIDTH = 320;
constexpr uint MINI_VIEWPORT_HEIGHT = 180;

DirectX::XMFLOAT4 camBackgroundColor = DirectX::XMFLOAT4(0.8f, 0.9f, 0.9f, 1.0f);

bool camera::init()
{
	transformPtr = new transform();

	uint width = e_globWindow.width();
	uint height = e_globWindow.height();

	screenViewport.topLeftX = 0.0f;
	screenViewport.topLeftY = 0.0f;
	screenViewport.width = static_cast<float>(width);
	screenViewport.height = static_cast<float>(height);

	scissor.left = 0;
	scissor.top = 0;
	scissor.right = static_cast<long>(width);
	scissor.bottom = static_cast<long>(height);

	transformPtr->setPosition(DirectX::XMVECTOR{ 0.0f,0.0f,2.0f });

	projectionBuffer = buf::createConstantBuffer(consts::CONST_PROJ_SIZE);

	desc = (render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, projectionBuffer));

	objectBuffer = buf::createConstantBuffer(consts::CONST_OBJ_SIZE);

	objectdesc = (render::getHeap(render::DESCRIPTORHEAP_BUFFER)->requestdescriptor(buf::BUFFER_CONSTANT_TYPE, objectBuffer));
	
	return true;
}

void camera::setCamAsMain()
{
	type = cam::CAMTYPE_MAIN;
}

void camera::draw(uint psoIndex, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
{
	//if(psoIndex == pso::PSO_DEBUG) 	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	//cmdList->SetGraphicsRootDescriptorTable(0, desc.getHandle());

	////TODO
	//world* world = game::getWorld();
	//uint* index = world->cameraObjectIndex;
	//uint objNum = world->cameraObjNum;
	//for (uint i = 0; i < objNum; ++i)
	//{
	//	object* obj = world->objects + index[i];
	//	if (obj->drawThisPSO(psoIndex))
	//	{
	//		obj->draw(cmdList, psoIndex == pso::PSO_DEBUG);
	//	}
	//}
}

void camera::close()
{
	delete transformPtr;
}

void camera::preDraw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList, bool wireframe)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT{ screenViewport.topLeftX, screenViewport.topLeftY, screenViewport.width, screenViewport.height };
	CD3DX12_RECT scissorRect = CD3DX12_RECT{ scissor.left, scissor.top, scissor.right, scissor.bottom };

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	render::getCmdQueue(render::QUEUE_GRAPHIC)->sendData(CBV_PROJECTION, desc.getHandle());
}

void camera::changeViewport(const cam::VIEWPORT_TYPE type)
{
	if (viewportType == type) return;

	uint width = e_globWindow.width();
	uint height = e_globWindow.height();

	if (type == cam::VIEWPORT_MINI)
	{
		screenViewport.topLeftX = static_cast<float>(width - MINI_VIEWPORT_WIDTH);
		screenViewport.topLeftY = static_cast<float>(height - MINI_VIEWPORT_HEIGHT);
		screenViewport.width = static_cast<float>(MINI_VIEWPORT_WIDTH);
		screenViewport.height = static_cast<float>(MINI_VIEWPORT_HEIGHT);

		scissor.left = width - MINI_VIEWPORT_WIDTH;
		scissor.top = height - MINI_VIEWPORT_HEIGHT;
		scissor.right = static_cast<long>(width);
		scissor.bottom = static_cast<long>(height);
	}
	else if (type == cam::VIEWPORT_FULL)
	{
		screenViewport.topLeftX = 0.0f;
		screenViewport.topLeftY = 0.0f;
		screenViewport.width = static_cast<float>(width);
		screenViewport.height = static_cast<float>(height);

		scissor.left = 0;
		scissor.top = 0;
		scissor.right = static_cast<long>(width);
		scissor.bottom = static_cast<long>(height);
	}

	viewportType = type;
}

void camera::updateView()
{
	DirectX::XMVECTOR up = transformPtr->getUP();
	DirectX::XMVECTOR right = transformPtr->getRIGHT();
	DirectX::XMVECTOR forward = DirectX::XMVector3Cross(up, right);

	DirectX::XMVECTOR camPos = transformPtr->getPosition();

	const float upScale = std::tanf(DirectX::XMConvertToRadians(FOV) * 0.5f);
	const float rightScale = upScale * (screenViewport.width / (float)screenViewport.height);

	frustum[0] = DirectX::XMVectorNegate(forward);
	frustum[0].m128_f32[3] = -DirectX::XMVector3Dot(frustum[0], DirectX::XMVectorMultiplyAdd(frustum[0], DirectX::XMVECTOR{ NEAR_PLANE, NEAR_PLANE, NEAR_PLANE }, camPos)).m128_f32[0];
	frustum[1] = forward;
	frustum[1].m128_f32[3] = -DirectX::XMVector3Dot(frustum[1], DirectX::XMVectorMultiplyAdd(frustum[1], DirectX::XMVECTOR{ FAR_PLANE, FAR_PLANE, FAR_PLANE }, camPos)).m128_f32[0];

	DirectX::XMVECTOR scaledUp = DirectX::XMVectorMultiply(up, DirectX::XMVECTOR{ upScale, upScale, upScale });
	DirectX::XMVECTOR scaledRight = DirectX::XMVectorMultiply(right, DirectX::XMVECTOR{ rightScale, rightScale, rightScale });

	DirectX::XMVECTOR topright = DirectX::XMVectorAdd(DirectX::XMVectorAdd(scaledRight, scaledUp), forward);
	DirectX::XMVECTOR bottomleft = DirectX::XMVectorAdd(DirectX::XMVectorNegate(DirectX::XMVectorAdd(scaledRight, scaledUp)), forward);

	frustum[2] = DirectX::XMVector3Cross(topright, scaledUp);
	frustum[2].m128_f32[3] = -DirectX::XMVector3Dot(frustum[2], camPos).m128_f32[0];

	frustum[3] = DirectX::XMVector3Cross(scaledRight, topright);
	frustum[3].m128_f32[3] = -DirectX::XMVector3Dot(frustum[3], camPos).m128_f32[0];

	frustum[4] = DirectX::XMVector3Cross(scaledUp, bottomleft);
	frustum[4].m128_f32[3] = -DirectX::XMVector3Dot(frustum[4], camPos).m128_f32[0];

	frustum[5] = DirectX::XMVector3Cross(bottomleft, scaledRight);
	frustum[5].m128_f32[3] = -DirectX::XMVector3Dot(frustum[5], camPos).m128_f32[0];
}

DirectX::XMVECTOR* camera::getFrustum()
{
	return frustum;
}

transform* camera::getTransform() const
{
	return transformPtr;
}

DirectX::XMMATRIX camera::getMat() const
{
	DirectX::XMVECTOR up = transformPtr->getUP();
	DirectX::XMVECTOR right = transformPtr->getRIGHT();

	DirectX::XMVECTOR forward = DirectX::XMVector3Cross(up, right);

	DirectX::XMVECTOR pos = transformPtr->getPosition();

	DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(pos, forward, up);
	DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(FOV), screenViewport.width / (float)screenViewport.height, NEAR_PLANE, FAR_PLANE);

	return view * projection;
}

void camera::toggleDebugMode()
{
	debugMode = !(debugMode);
	if (debugMode) changeViewport(cam::VIEWPORT_MINI);
	else changeViewport(cam::VIEWPORT_FULL);
}

void camera::update(float dt)
{
	//if (!(debugMode) && type == cam::CAMTYPE_DEBUG) return;

	DirectX::XMVECTOR rotation = transformPtr->getQuaternion();

	DirectX::XMVECTOR up = transformPtr->getUP();
	DirectX::XMVECTOR right = transformPtr->getRIGHT();

	DirectX::XMVECTOR forward = DirectX::XMVector3Cross(up, right);

	DirectX::XMVECTOR pos = transformPtr->getPosition();

	DirectX::XMMATRIX view = DirectX::XMMatrixLookToRH(pos, forward, up);

	DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(FOV), screenViewport.width / (float)screenViewport.height, NEAR_PLANE, FAR_PLANE);

	DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, projection);

	memcpy(projectionBuffer->info.cbvDataBegin, &viewProj, sizeof(float) * 4 * 4);
	memcpy(projectionBuffer->info.cbvDataBegin + sizeof(float) * 4 * 4, &pos, sizeof(float) * 3);
	memcpy(projectionBuffer->info.cbvDataBegin + sizeof(float) * 4 * 4 + sizeof(float) * 3, &FAR_PLANE, sizeof(float));
	
	if (viewportType == cam::VIEWPORT_MINI) return;

	float x = 0;
	float y = 0;
	int mouseMovex = 0;
	int mouseMovey = 0;

	if (input::isPressed(input::KEY_LEFT)) x -= 1.f;
	if (input::isPressed(input::KEY_RIGHT)) x += 1.f;
	if (input::isPressed(input::KEY_UP)) y += 1.f;
	if (input::isPressed(input::KEY_DOWN)) y -= 1.f;

	if (input::isPressed(input::KEY_LEFTSHIFT))
	{
		x *= 5.0f;
		y *= 5.0f;
	}

	if (input::isPressed(input::KEY_LEFTCTRL))
	{
		x *= 0.2f;
		y *= 0.2f;
	}

	if (input::isPressed(input::KEY_RIGHTCLICK))
	{
		input::pos mouseMove = input::getMouseMove();

		mouseMovex = mouseMove.x;
		mouseMovey = mouseMove.y;
	}

	move(x, y, mouseMovex, mouseMovey, dt);
}

void camera::move(float x, float y, int mousex, int mousey, float dt)
{
	if (x != 0)
	{
		DirectX::XMVECTOR right = transformPtr->getRIGHT();

		right = DirectX::XMVectorScale(right, x * dt * SPEED);

		transformPtr->movePosition(right);
	}

	if (y != 0)
	{
		DirectX::XMVECTOR up = transformPtr->getUP();
		DirectX::XMVECTOR right = transformPtr->getRIGHT();

		DirectX::XMVECTOR forward = DirectX::XMVector3Cross(up, right);

		forward = DirectX::XMVectorScale(forward, y * dt * SPEED);

		transformPtr->movePosition(forward);
	}

	static float pitch = 0.0f;
	static float yaw = 0.0f;

	if (mousex != 0.0f)
	{
		yaw -= 0.01f * mousex;

		transformPtr->setRotation(DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, 0.0f));
	}

	if (mousey != 0.0f)
	{
		pitch += 0.01f * mousey;

		pitch = (pitch > -PI_HALF) ? pitch : -PI_HALF;
		pitch = (pitch < PI_HALF) ? pitch : PI_HALF;

		transformPtr->setRotation(DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, 0.0f));
	}
}