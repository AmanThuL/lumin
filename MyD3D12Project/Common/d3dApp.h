//*******************************************************************
// Copyright Frank Luna (C) 2015 All Rights Reserved.
//
// d3dApp.h:
//
// The core Direct3D application class code that is used to encapsulate 
// a Direct3D sample application.
//*******************************************************************

#pragma once

// When the _CRTDBG_MAP_ALLOC flag is defined in the debug version of an 
// application, the base version of the heap functions are directly mapped to 
// their debug versions.
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

	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;

	virtual ~D3DApp();

public:

	// Static requirements for OS-level message processing
	static D3DApp* GetAppInstance();
	static LRESULT CALLBACK MainWndProc(
		HWND hwnd,			// Window handle
		UINT msg,			// Message
		WPARAM wParam,		// Message's first parameter
		LPARAM lParam);		// Message's second parameter

	// Trivial access functions
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

	bool Get4xMsaaState()const;
	void Set4xMsaaState(bool value);

	int Run();

	// Framework methods for override
	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize();

	// Pure virtual methods for setup and game functionality
	virtual void Update(const GameTimer& gt) = 0;
	virtual void Draw(const GameTimer& gt) = 0;

	// Convenience methods for handling mouse input, since we
	// can easily grab mouse input from OS-level messages
	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }
	virtual void OnMouseWheel(float wheelDelta, int x, int y) { }

protected:
	// Initialization methods
	bool InitMainWindow();
	bool InitDirect3D();
	void CheckMaxFeatureSupport();
	void CreateCommandObjects();
	void CreateSwapChain();

	// Helper function for allocating a console window
	void CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns);

	// CPU/GPU Synchronization method
	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
	static D3DApp* mAppInstance;

	// Used to keep track of the “delta-time” and game time (§4.4).
	GameTimer mTimer;

	HINSTANCE mhAppInst = nullptr;		// Application instance handle
	HWND      mhMainWnd = nullptr;		// Main window handle
	bool      mAppPaused = false;		// Is the application paused?
	bool      mMinimized = false;		// Is the application minimized?
	bool      mMaximized = false;		// Is the application maximized?
	bool      mResizing = false;		// Are the resize bars being dragged?
	bool      mFullscreenState = false;	// Fullscreen enabled

	// Set true to use 4X MSAA (§4.1.8).  The default is false.
	bool      m4xMsaaState = false;    // 4X MSAA enabled
	UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	D3D_FEATURE_LEVEL dxFeatureLevel;

	Microsoft::WRL::ComPtr<IDXGIFactory4>				mdxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain>				mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device>				md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence>					mFence;
	UINT64 mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource>				mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring	mMainWndCaption = L"DirectX Rendering Engine";
	bool			titleBarStats = true;	// Show extra stats in title bar?
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT		mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT		mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int				mClientWidth = 1600;
	int				mClientHeight = 900;

private:
	void UpdateTitleBarStats();	// Puts debug info in the title bar
};

