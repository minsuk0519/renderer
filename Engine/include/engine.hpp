#pragma once

#include <Windows.h>

class engine
{
private:

public:
	bool init(HINSTANCE hInstance, int nCmdShow, LPSTR cmdArgs);

	void run();

	void close();
};

extern engine e_globEngine;