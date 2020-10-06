/**
	The core Direct3D application class code that is
	used to encapsulate a Direct3D sample application.

	@file d3dApp.h
	@author Frank Luna
	@author Rudy Zhang
	@copyright 2020 All Rights Reserved.
*/

#pragma once

// When the _CRTDBG_MAP_ALLOC flag is defined in the debug version of an application, 
// the base version of the heap functions are directly mapped to their debug versions.
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3DApp
{
protected:

	/// <summary>
	/// Constructor that initializes the data members to default values.
	/// </summary>
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;

	/// <summary>
	/// Destructor releases the COM interfaces the D3DApp acquires, and flushes the command queue.
	/// </summary>
	virtual ~D3DApp();

public:

	static D3DApp* GetApp();

	/// <returns>Returns a copy of the application instance handle.</returns>
	HINSTANCE AppInst()const;
	/// <returns>Returns a copy of the main window handle.</returns>
	HWND      MainWnd()const;
	/// <returns>Returns the ratio of the back buffer width to its height.</returns>
	float     AspectRatio()const;

	/// <returns>Returns true is 4X MSAA is enabled and false otherwise.</returns>
	bool Get4xMsaaState()const;
	/// <summary>Enables/disables 4X MSAA.</summary>
	void Set4xMsaaState(bool value);

	/// <summary>This method wraps the application message loop.</summary>
	int Run();

	/// <summary>Initialize the application such as allocating resources, initializing objects, and setting up the 3D scene.</summary>
	virtual bool Initialize();
	/// <summary>
	/// Implements the window procedure function for the main application window.
	/// Only need to override this method if there is a message that needs to be handled and D3DApp::MsgProc does not handle.
	/// </summary>
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	/// <summary>Virtual function where you create the RTV and DSV descriptor heaps your application needs.</summary>
	virtual void CreateRtvAndDsvDescriptorHeaps();
	/// <summary>
	/// Called when a WM_SIZE message is received. 
	/// Some Direct3D properties need to be changed when the window is resized.
	/// </summary>
	virtual void OnResize();
	/// <summary>This abstract method is called every frame and should be used to update the 3D application over time 
	/// (e.g. perform animations, move the camera, do collision detection, check for user input, and etc.).
	/// </summary>
	virtual void Update(const GameTimer& gt) = 0;
	/// <summary>
	/// This abstract method is invoked every frame and is where rendering commands are issued to draw the current frame to the back buffer.
	/// </summary>
	virtual void Draw(const GameTimer& gt) = 0;

	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

protected:

	/// <summary>Initializes the main application window.</summary>
	bool InitMainWindow();
	/// <summary>Initializes Direct3D by steps.</summary>
	bool InitDirect3D();
	/// <summary>Creates the command queue, a command list allocator, and a command list.</summary>
	void CreateCommandObjects();
	/// <summary>Creates the swap chain and allows to recreate swap chain with different settings.</summary>
	void CreateSwapChain();

	/// <summary>
	/// Forces the CPU to wait until the GPU has finished processing all the commands in the queue.
	/// </summary>
	void FlushCommandQueue();

	/// <returns>Returns an ID3D12Resource to the current back buffer in the swap chain.</returns>
	ID3D12Resource* CurrentBackBuffer()const;
	/// <returns>Returns the RTV (render target view) to the current back buffer.</returns>
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	/// <returns>Returns the DSV (depth/stencil view) to the main depth/stencil buffer.</returns>
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	/// <summary>Calculates the average frames per second and the average milliseconds per frame.</summary>
	void CalculateFrameStats();

	/// <summary>Enumerates all the adapters on a system.</summary>
	void LogAdapters();
	/// <summary>Enumerates all the outputs associated with an adapter.</summary>
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	/// <summary>Enumerates all the display modes an output supports for a given format.</summary>
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:

	static D3DApp* mApp;

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND      mhMainWnd = nullptr; // main window handle
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled

	// Set true to use 4X MSAA (§4.1.8).  The default is false.
	bool      m4xMsaaState = false;    // 4X MSAA enabled
	UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	// Used to keep track of the “delta-time” and game time (§4.4).
	GameTimer mTimer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"d3d App";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;
};

