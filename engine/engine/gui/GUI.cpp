//*******************************************************************
// GUI.cpp
//*******************************************************************

#include "GUI.h"

#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> imFontHeap = nullptr;

void GUI::Initialize(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC imFontHeapDesc = {};
    imFontHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    imFontHeapDesc.NodeMask = 0;
    imFontHeapDesc.NumDescriptors = 1;
    imFontHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    ThrowIfFailed(device->CreateDescriptorHeap(&imFontHeapDesc, IID_PPV_ARGS(&imFontHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE imFontCPUHandle(imFontHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_GPU_DESCRIPTOR_HANDLE imFontGPUHandle = imFontHeap->GetGPUDescriptorHandleForHeapStart();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); 
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    bool init1 = ImGui_ImplWin32_Init(D3DApp::GetAppInstance()->MainWnd());
    assert("ImGui Win32 Init Failed.");

    bool init2 = ImGui_ImplDX12_Init(device, gNumFrameResources,
        DXGI_FORMAT_R8G8B8A8_UNORM, imFontHeap.Get(),
        imFontCPUHandle, imFontGPUHandle);
    assert("ImGui DX12 Init Failed.");

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontFromFileTTF("../../resources/fonts/FiraCode-Retina.ttf", 16.0f);
}

void GUI::ShutDown()
{
    // Clean up
    ImGui_ImplDX12_Shutdown();
    //ImGui_ImplWin32_Shutdown();
    //ImGui::DestroyContext();
}

void GUI::StartFrame()
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        static int imCounter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            imCounter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", imCounter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

}

void GUI::RenderFrame(ID3D12GraphicsCommandList* commandList)
{
    // Set the descriptor heaps to the command list.
    ID3D12DescriptorHeap* descriptorHeaps[] = { imFontHeap.Get() };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    // Rendering
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}


