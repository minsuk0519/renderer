#pragma once

#include <Windows.h>
#include <string>

#include "defines.hpp"

class window
{
private:
	uint screenWidth;
	uint screenHeight;

	std::string windowTitle;

	HWND hWindow = nullptr;

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	bool init(HINSTANCE hInstance, int nCmdShow, uint width, uint height);

	void run();

	void close();

	HWND getWindow() const;

	uint width() const;
	uint height() const;
};

extern window e_globWindow;