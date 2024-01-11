#pragma once

#include <system\defines.hpp>

namespace input
{
	struct pos
	{
		int x;
		int y;
	};

	enum KEY_NAME
	{
		KEY_INVALID = 0,
		KEY_ESC,
		KEY_UP,
		KEY_DOWN,
		KEY_LEFT,
		KEY_RIGHT,
		KEY_SPACE,
		KEY_RIGHTCLICK,
		KEY_LEFTCLICK,
		KEY_LEFTSHIFT,
		KEY_LEFTCTRL,

		KEY_MAX,
	};

	void init();

	void keyEvent(int keyCode, bool down);

	void update();

	bool isPressed(int keyCode);
	bool isTriggered(int keyCode);
	bool isReleased(int keyCode);

	pos getMousePos();
	pos getMouseMove();
}