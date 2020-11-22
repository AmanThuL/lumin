//*******************************************************************
// Main.cpp:
//
// This is where the application entry point resides. It turns on
// memory leak detection (in debug builds), creates one & only App
// class object, initializes it and enters the App loop.
//*******************************************************************

#include <Windows.h>
#include "ShapesApp.h"

// --------------------------------------------------------
// Entry point for a graphical (non-console) Windows application
// --------------------------------------------------------
int WINAPI WinMain(
	_In_ HINSTANCE hInstance, 
	_In_opt_ HINSTANCE prevInstance,
	_In_ PSTR cmdLine, 
	_In_ int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	// Enable memory leak detection as a quick and dirty
	// way of determining if we forgot to clean something up
	//  - You may want to use something more advanced, like Visual Leak Detector
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		// Create the Game object using
		// the app handle we got from WinMain
		ShapesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}
