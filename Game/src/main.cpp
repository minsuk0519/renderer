#include <engine.hpp>
#include <system\logger.hpp>

#define MEMLEAK 0

#if MEMLEAK

//for memory debug
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define _CRTDBG_MAP_ALLOC
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)

#include <crtdbg.h>

#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR cmdArgs, int nCmdShow)
{
#ifdef _DEBUG
	AllocConsole();
#endif // #ifdef _DEBUG

	TC_INIT(e_globEngine.init(hInstance, nCmdShow, cmdArgs));

	e_globEngine.run();

	e_globEngine.close();

	return 0;
}