//*******************************************************************
// GUI.h:
//
// A wrapper class for handling ImGui integration.
//*******************************************************************

#pragma once

#include "common/d3dApp.h"

// ImGUI libraries
#include "imgui/imgui.h"

namespace GUI
{
	using namespace ImGui;

	void Initialize(ID3D12Device* device);
	void ShutDown();

	void StartFrame();
	void RenderFrame(ID3D12GraphicsCommandList* commandList);
}