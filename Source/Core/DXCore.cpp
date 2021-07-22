//*******************************************************************
// d3dApp.cpp
//*******************************************************************
#include "lmpch.h"
#include "DXCore.h"

#include <Windowsx.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

const int gNumFrameResources = 3;

// Define the static instance variable so our OS-level 
// message handling function below can talk to our object
DXCore* DXCore::mDXCoreInstance = nullptr;

// ------------------------------------------------------------------
// The global callback function for handling windows OS-level messages.
//
// This needs to be a global function (not part of a class), but we want
// to forward the parameters to our class to properly handle them.
// ------------------------------------------------------------------
LRESULT DXCore::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return DXCore::GetDXCoreInstance()->ProcessMessage(hwnd, msg, wParam, lParam);
}

// ------------------------------------------------------------------
// Return a reference to the D3DApp instance.
// ------------------------------------------------------------------
DXCore* DXCore::GetDXCoreInstance()
{
	return mDXCoreInstance;
}

// ------------------------------------------------------------------
// Constructor - Initializes the data members to default values
//
// hInstance   - The application's OS-level handle (unique ID)
// ------------------------------------------------------------------
DXCore::DXCore(HINSTANCE hInstance)
	: mhCoreInst(hInstance)
{
	// Save a static reference to this object.
	//  - Since the OS-level message function must be a non-member (global) 
	//    function, it won't be able to directly interact with our D3DApp 
	//    object otherwise.
	//  - (Yes, a singleton might be a safer choice here).
	assert(mDXCoreInstance == nullptr);
	mDXCoreInstance = this;

	// Setup ImGui
	GUI::Init();
}

// ------------------------------------------------------------------
// Destructor - Release the COM interfaces the D3DApp acquires and 
// flushes the command queue
// ------------------------------------------------------------------
DXCore::~DXCore()
{
	// Wait until the GPU is done processing the commands in the queue 
	// before we destroy any resource the GPU is still referencing.
	// Otherwise, the GPU might crash when the application exits.
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

// ------------------------------------------------------------------
// Returns a copy of the application instance handle.
// ------------------------------------------------------------------
HINSTANCE DXCore::CoreInst() const
{
	return mhCoreInst;
}

// ------------------------------------------------------------------
// Returns a copy of the main window handle.
// ------------------------------------------------------------------
HWND DXCore::MainWnd()const
{
	return mhMainWnd;
}

// ------------------------------------------------------------------
// Returns the ratio of the back buffer width to its height.
// ------------------------------------------------------------------
float DXCore::AspectRatio() const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

// ------------------------------------------------------------------
// Returns true is 4X MSAA is enabled and false otherwise.
// ------------------------------------------------------------------
bool DXCore::Get4xMsaaState() const
{
	return m4xMsaaState;
}

// ------------------------------------------------------------------
// Enables/disables 4X MSAA.
// ------------------------------------------------------------------
void DXCore::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		OnResize();
	}
}

// ------------------------------------------------------------------
// This method wraps the application message loop.
// ------------------------------------------------------------------
int DXCore::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT && GUI::IsWndActive())
	{
		// If there are Window messages then process them.
		// Uses PeekMessage() so that it can process our game logic when no
		// messages are present
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				if (titleBarStats)
					UpdateTitleBarStats();

				// The game loop
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}


#pragma region Framework Methods

// ------------------------------------------------------------------
// Initialize the application such as allocating resources, initializing 
// objects, and setting up the 3D scene.
// ------------------------------------------------------------------
bool DXCore::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	// Do the initial resize code.
	OnResize();

	return true;
}

// ------------------------------------------------------------------
// Implements the window procedure function for the main application 
// window. Only need to override this method if there is a message that 
// needs to be handled and MsgProc does not handle.
// ------------------------------------------------------------------
LRESULT DXCore::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	GUI::SetupWndProcHandler(MainWnd(), msg, wParam, lParam);

	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from 
					// dragging the resize bars.  So instead, we reset after 
					// the user is done resizing the window and releases the 
					// resize bars, which sends a WM_EXITSIZEMOVE message.
				}
				else
				{
					// API call such as SetWindowPos or 
					// mSwapChain->SetFullscreenState.
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user 
		// presses a key that does not correspond to any mnemonic or 
		// accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!m4xMsaaState);

		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}


// ------------------------------------------------------------------
// Virtual function where you create the RTV and DSV descriptor heaps 
// your application needs.
// ------------------------------------------------------------------
void DXCore::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

// ------------------------------------------------------------------
// Called when a WM_SIZE message is received. Some Direct3D properties 
// need to be changed when the window is resized.
// ------------------------------------------------------------------
void DXCore::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer
	// to read from the depth buffer. Therefore, because we need to create two 
	// views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of 
	// the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth 
	// buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

#pragma endregion

// ------------------------------------------------------------------
// Initializes the main application window.
// ------------------------------------------------------------------
bool DXCore::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhCoreInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area 
	// dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhCoreInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	GUI::SetupWnd(mhMainWnd);

	return true;
}

// ------------------------------------------------------------------
// Initializes Direct3D by steps.
// ------------------------------------------------------------------
bool DXCore::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
	// Enable the D3D12 debug layer for debug mode builds.
	// Direct3D will enable extra debugging and send debug messages to the VC++
	// output window.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// 1 - Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,  // specifying null uses the primary display adapter
		D3D_FEATURE_LEVEL_11_0,  // Direct3D 11 feature support
		IID_PPV_ARGS(&md3dDevice));  // The COM ID of the ID3D12Device 

	// If call to D3D12CreateDevice fails, we fallback to a WARP device 
	// (Windows Advanced Rasterization Platform, a software adapter).
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	// Check for max supported feature level
	CheckMaxFeatureSupport();

	// 2 - Create the Fence and Descriptor Sizes.
	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	// Descriptor sizes can vary across GPUs so we need to query this 
	// information. We cache the descriptor sizes so that it is available when 
	// we need it for various types.
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 3 - Check 4X MSAA quality support for our back buffer format.
	// 4X gives a good improvement without being overly expensive.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	// Because 4X MSAA is always supported, 
	// the returned quality should always be greater than 0.
	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
	LogAdapters();
#endif

	// 4 - Create Command Queue and Command List
	CreateCommandObjects();

	// 5 - Describe and Create the Swap Chain
	CreateSwapChain();

	// 6 - Create the Descriptor Heaps
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

// ------------------------------------------------------------------
// Check the maximum supported feature level.
// ------------------------------------------------------------------
void DXCore::CheckMaxFeatureSupport()
{
	D3D_FEATURE_LEVEL featureLevels[4] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelsInfo;
	featureLevelsInfo.NumFeatureLevels = 4;
	featureLevelsInfo.pFeatureLevelsRequested = featureLevels;
	md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_FEATURE_LEVELS,
		&featureLevelsInfo,
		sizeof(featureLevelsInfo));

	dxFeatureLevel = featureLevelsInfo.MaxSupportedFeatureLevel;
}

// ------------------------------------------------------------------
// Creates the command queue, a command list allocator, and a command 
// list.
// ------------------------------------------------------------------
void DXCore::CreateCommandObjects()
{
	// The CPU submits commands to the GPU's command queue through the Direct3D 
	// API using command lists.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	// As commands are recorded to the command list, they will be stored in the
	// associated command allocator. When a command list is executed, the 
	// command queue will reference the commands in the allocator.
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,  // store a list of commands to directly be executed by the GPU
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,  // Set to 0 for single GPU system
		D3D12_COMMAND_LIST_TYPE_DIRECT,  // Type of command list
		mDirectCmdListAlloc.Get(),  // Associated command allocator (allocator type must match list type)
		nullptr,  // Initial pipeline state of the command list
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	// All command lists must be closed except the one whose commands we are 
	// going to record.
	// Create/Reset a command list = it is in an "open" state.
	mCommandList->Close();
}

// ------------------------------------------------------------------
// Creates the swap chain and allows to recreate swap chain with 
// different settings.
// ------------------------------------------------------------------
void DXCore::CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;

	// BufferDesc - the properties of the back buffer we want to create
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// SampleDesc - the number of multisamples and quality level
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

	// Use the back buffer as a render target
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// Two for double buffering
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true; // true = windowed, false = full-screen
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// Current flag - when the application is switching to full-screen mode,
	// it will choose a display mode that best matches the current application 
	// window dimensions.
	// Flag not specified - when the application is switching to full-screen 
	// mode, it will use the current desktop display mode.
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

// ------------------------------------------------------------------
// Allocates a console window we can print to for debugging
// 
// bufferLines   - Number of lines in the overall console buffer
// bufferColumns - Numbers of columns in the overall console buffer
// windowLines   - Number of lines visible at once in the window
// windowColumns - Number of columns visible at once in the window
// ------------------------------------------------------------------
void DXCore::CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns)
{
	// Our temp console info struct
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// Get the console info and set the number of lines
	AllocConsole();
	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&coninfo);
	coninfo.dwSize.Y = bufferLines;
	coninfo.dwSize.X = bufferColumns;
	SetConsoleScreenBufferSize(
		GetStdHandle(STD_OUTPUT_HANDLE),
		coninfo.dwSize);

	SMALL_RECT rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = windowColumns;
	rect.Bottom = windowLines;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &rect);

	FILE* stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w", stdout);
	freopen_s(&stream, "CONOUT$", "w", stderr);

	// Prevent accidental console window close
	HWND consoleHandle = GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(consoleHandle, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
}



// ------------------------------------------------------------------
// Forces the CPU to wait until the GPU has finished processing all the 
// commands in the queue.
// ------------------------------------------------------------------
void DXCore::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be set 
	// until the GPU finishes processing all the commands prior to this 
	// Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

// ------------------------------------------------------------------
// Returns an ID3D12Resource to the current back buffer in the swap 
// chain.
// ------------------------------------------------------------------
ID3D12Resource* DXCore::CurrentBackBuffer() const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

// ------------------------------------------------------------------
// Returns the RTV(render target view) to the current back buffer.
// ------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE DXCore::CurrentBackBufferView() const
{
	// CD3DX12 constructor to offset to the RTV of the current back buffer.
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(), // handle start
		mCurrBackBuffer,  // index to offset
		mRtvDescriptorSize);  // byte size of descriptor
}

// ------------------------------------------------------------------
// Returns the DSV (depth/stencil view) to the main depth/stencil buffer.
// ------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE DXCore::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

// ------------------------------------------------------------------
// Calculates the average frames per second and the average milliseconds 
// per frame.
// ------------------------------------------------------------------
void DXCore::UpdateTitleBarStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Only calc FPS and upate title bar once per second
	float timeDiff = mTimer.TotalTime() - timeElapsed;
	if (timeDiff < 1.0f)
		return;

	// Update FPS and MSPF values
	float fps = (float)frameCnt; // fps = frameCnt / 1
	float mspf = 1000.0f / fps;

	GUI::SetFrameTime(fps, mspf);

	// Quick and dirty title bar text (mostly for debugging)
	std::wostringstream output;
	output.precision(6);
	output << mMainWndCaption;

	// Append the version of DirectX the app is using
	switch (dxFeatureLevel)
	{
	case D3D_FEATURE_LEVEL_12_1: output << L"   <DX12 (FL 12.1)>"; break;
	case D3D_FEATURE_LEVEL_12_0: output << L"   <DX12 (FL 12.0)>"; break;
	case D3D_FEATURE_LEVEL_11_1: output << L"   <DX12 (FL 11.1)>"; break;
	case D3D_FEATURE_LEVEL_11_0: output << L"   <DX12 (FL 11.0)>"; break;
	default:                     output << L"   <???>";  break;
	}

	SetWindowText(mhMainWnd, output.str().c_str());

	// Reset for next average.
	frameCnt = 0;
	timeElapsed += 1.0f;

}

// ------------------------------------------------------------------
// Enumerates all the adapters on a system.
// ------------------------------------------------------------------
void DXCore::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

// ------------------------------------------------------------------
// Enumerates all the outputs associated with an adapter.
// ------------------------------------------------------------------
void DXCore::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

// ------------------------------------------------------------------
// Enumerates all the display modes an output supports for a given 
// format.
// ------------------------------------------------------------------
void DXCore::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}