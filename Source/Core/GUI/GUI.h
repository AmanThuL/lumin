//*******************************************************************
// GUI.h:
//
// A wrapper class for handling ImGui integration.
//*******************************************************************

#pragma once

// ImGui libraries
#include "DescriptorHeap.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

namespace GUI
{
	using namespace ImGui;

	void Init();
	void SetupWnd(HWND);
	void SetupWndProcHandler(HWND, UINT, WPARAM, LPARAM);
	void SetupRenderer(ID3D12Device*, DescriptorHeapWrapper*);

	void StartFrame();
	void RenderFrame(ID3D12GraphicsCommandList*, DescriptorHeapWrapper*);

	void ShutDown();

	// Custom methods
	bool IsWndActive();
	void SetFrameTime(float, float);

	static bool active = true;
	static float fps;
	static float mspf;
}