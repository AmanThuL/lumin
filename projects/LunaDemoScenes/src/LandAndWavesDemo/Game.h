//*******************************************************************
// LandAndWaves.h:
//
// - Constructs a triangle grid mesh procedurally and offsets the
//   heights to create a terrain.
// - Uses another triangle grid to represent water, and animates the
//   vertex heights to create waves.
// - Switches to using root descriptors for constant buffers, which
//   allows us to drop support for a descriptor heap for CBVs.
// - Hold down '1' key to view scene in wireframe mode.
//*******************************************************************

#pragma once

#include "common/DXCore.h"
#include "common/MathHelper.h"
#include "common/UploadBuffer.h"
#include "common/GeometryGenerator.h"
#include "common/Camera.h"

#include "GeoBuilder.h"

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Sky,
	Count
};

class Game : public DXCore
{
public:

	Game(HINSTANCE hInstance);
	Game(const Game& rhs) = delete;
	Game& operator=(const Game& rhs) = delete;
	~Game();

	virtual bool Initialize()override;

private:

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void OnMouseWheel(float wheelDelta, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateInstanceData(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawGUI();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unique_ptr<DescriptorHeapWrapper> mCbvSrvUavDescriptorHeap = nullptr;
	std::unique_ptr<TextureWrapper> mTextures = nullptr;
	std::unique_ptr<GeoBuilder> mGeoBuilder = nullptr;

	// Use unordered maps for constant time lookup and reference our objects by 
	// name.
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// Render items.
	RenderItem* mWavesRitem = nullptr;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	// Instance count
	std::vector<UINT> mInstanceCounts;  // Max instance counts of all render items
	int totalVisibleInstanceCount = 0;
	int totalInstanceCount = 0;

	// Application-level frustum culling
	bool mFrustumCullingEnabled = true;
	DirectX::BoundingFrustum mCamFrustum;

	UINT mSkyTexHeapIndex = 0;

	PassConstants mMainPassCB;

	bool mIsWireframe = false;

	Camera mCamera;
	DirectX::XMFLOAT3 mDefaultCamPos = { 15.0f, 18.0f, -78.0f };

	// Keeps track of the old mouse position. Useful for determining how far 
	// the mouse moved in a single frame.
	POINT mLastMousePos;
};

