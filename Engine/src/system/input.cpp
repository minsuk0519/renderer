#include <system\input.hpp>
#include <system\window.hpp>

#include <bitset>

#include <Windows.h>

namespace input
{
	constexpr uint KEY_LIST_SIZE = 255;
	
	int KEY_LIST[KEY_LIST_SIZE] = {
		KEY_INVALID,
	};

	std::bitset<KEY_MAX> keyPressed;
	std::bitset<KEY_MAX> keyTriggered;

	pos mousePos;
	pos preMousePos;
	pos mouseMove;

	POINT pointMouse;
};

void input::init()
{
	//KEY_ESC
	KEY_LIST[VK_ESCAPE] = KEY_ESC;
	//KEY_UP
	KEY_LIST[VK_UP] = KEY_UP;
	KEY_LIST[/*VK_W*/0x57] = KEY_UP;
	//KEY_DOWN
	KEY_LIST[VK_DOWN] = KEY_DOWN;
	KEY_LIST[/*VK_S*/0x53] = KEY_DOWN;
	//KEY_LEFT
	KEY_LIST[VK_LEFT] = KEY_LEFT;
	KEY_LIST[/*VK_A*/0x41] = KEY_LEFT;
	//KEY_RIGHT
	KEY_LIST[VK_RIGHT] = KEY_RIGHT;
	KEY_LIST[/*VK_D*/0x44] = KEY_RIGHT;
	//KEY_SPACE
	KEY_LIST[VK_SPACE] = KEY_SPACE;

	KEY_LIST[VK_LBUTTON] = KEY_LEFTCLICK;
	KEY_LIST[VK_RBUTTON] = KEY_RIGHTCLICK;

	KEY_LIST[VK_SHIFT] = KEY_LEFTSHIFT;

	KEY_LIST[VK_CONTROL] = KEY_LEFTCTRL;

	keyPressed.reset();
	keyTriggered.reset();
}

void input::keyEvent(int keyCode, bool down)
{
	if (down)
	{
		keyPressed[KEY_LIST[keyCode]] = true;
		keyTriggered[KEY_LIST[keyCode]] = true;
	}
	else keyPressed[KEY_LIST[keyCode]] = false;
}

void input::update()
{
	GetCursorPos(&pointMouse);

	ScreenToClient(e_globWindow.getWindow(), &pointMouse);

	int width = (int)e_globWindow.width();
	int height = (int)e_globWindow.height();

	mousePos = { pointMouse.x, height - pointMouse.y };

	if (mousePos.x < 0) mousePos.x = 0;
	if (mousePos.x > width) mousePos.x = width;
	if (mousePos.y < 0) mousePos.y = 0;
	if (mousePos.y > height) mousePos.y = height;

	mouseMove.x = mousePos.x - preMousePos.x;
	mouseMove.y = mousePos.y - preMousePos.y;

	preMousePos = mousePos;

	keyTriggered.reset();
}

bool input::isPressed(int keyCode)
{
	return keyPressed[keyCode];
}

bool input::isTriggered(int keyCode)
{
	return keyTriggered[keyCode];
}

bool input::isReleased(int keyCode)
{
	return !keyPressed[keyCode];
}

input::pos input::getMousePos()
{
	return mousePos;
}

input::pos input::getMouseMove()
{
	return mouseMove;
}

/*
0x41	A
0x42	B
0x43	C
0x44	D
0x45	E
0x46	F
0x47	G
0x48	H
0x49	I
0x4A	J
0x4B	K
0x4C	L
0x4D	M
0x4E	N
0x4F	O
0x50	P
0x51	Q
0x52	R
0x53	S
0x54	T
0x55	U
0x56	V
0x57	W
0x58	X
0x59	Y
0x5A	Z
*/