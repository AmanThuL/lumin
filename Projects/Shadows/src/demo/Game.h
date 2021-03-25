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

#include "Common/DXCore.h"
#include "Common/UploadBuffer.h"
#include "Common/Camera.h"

#include "RenderPasses/ShadowMap.h"

#include "GeoBuilder.h"
#include "Material.h"

enum class RenderLayer : int
{
	Opaque = 0,
	Debug,
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

	virtual void CreateRtvAndDsvDescriptorHeaps()override;
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
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();

	void DrawSceneToShadowMap();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawGUI();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unique_ptr<DescriptorHeapWrapper> mCbvSrvUavDescriptorHeap = nullptr;
	std::unique_ptr<TextureWrapper> mTextures = nullptr;
	std::unique_ptr<GeoBuilder> mGeoBuilder = nullptr;
	std::unique_ptr<MaterialWrapper> mMaterials = nullptr;

	// Use unordered maps for constant time lookup and reference our objects by 
	// name.
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// Render items.
	RenderItem* mWavesRitem = nullptr;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	// Instancing variables
	std::vector<UINT> mInstanceCounts;  // Max instance counts of all render items
	int totalVisibleInstanceCount = 0;
	int totalInstanceCount = 0;

	// Application-level frustum culling
	bool mFrustumCullingEnabled = false;
	DirectX::BoundingFrustum mCamFrustum;

	UINT mSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;
	UINT mNullCubeSrvIndex = 0;
	UINT mNullTexSrvIndex = 0;

	// Constant buffer for different rendering passes
	PassConstants mMainPassCB;  // index 0 of pass cbuffer.
	PassConstants mShadowPassCB;// index 1 of pass cbuffer.

	bool mIsWireframe = false;

	Camera mCamera;
	DirectX::XMFLOAT3 mDefaultCamPos = { 15.0f, 18.0f, -78.0f };

	std::unique_ptr<ShadowMap> mShadowMap;

	// The light view volume is computed to fit the bounding sphere of the
	// entire scene.
	DirectX::BoundingSphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	DirectX::XMFLOAT3 mLightPosW;
	DirectX::XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	DirectX::XMFLOAT3 mBaseLightDirections[3] = {
		DirectX::XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		DirectX::XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		DirectX::XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	DirectX::XMFLOAT3 mRotatedLightDirections[3];

	// Keeps track of the old mouse position. Useful for determining how far 
	// the mouse moved in a single frame.
	POINT mLastMousePos;
};

