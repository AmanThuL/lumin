//*******************************************************************
// FrameResource.h:
//
// Creates a circular array of the resources the CPU needs to modify
// each frame.
//*******************************************************************

#pragma once

#include "Utils/DXUtil.h"
#include "Math/MathHelper.h"
#include "UploadBuffer.h"

#include "Material.h"

// Stores data that varies per-instance.
struct InstanceData
{
    DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    UINT MaterialIndex;
    UINT InstancePad0;
    UINT InstancePad1;
    UINT InstancePad2;
};

// Stores constant data that is fixed over a given rendering pass.
struct PassConstants
{
    DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;

    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    DirectX::XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
    float gFogStart = 5.0f;
    float gFogRange = 150.0f;
    DirectX::XMFLOAT2 cbPerObjectPad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light Lights[MaxLights];
};

struct Vertex
{
    DirectX::XMFLOAT3 Pos;

    // Lighting calculations require a surface normal.
    DirectX::XMFLOAT3 Normal;

    DirectX::XMFLOAT2 TexC;
    DirectX::XMFLOAT3 TangentU;
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.
struct FrameResource
{
public:

    // Constructors
    FrameResource(ID3D12Device* device, UINT passCount, std::vector<UINT> maxInstanceCounts, UINT materialCount, UINT waveVertCount = 0);

    FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	// We cannot reset the allocator until the GPU is done processing the
	// commands. So each frame needs their own allocator.
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// We cannot update a cbuffer until the GPU is done processing the
	// commands that reference it. So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

    // In this demo, we instance only one render-item, so we only have one 
    // structured buffer to store instancing data.
    // To make this more general (i.e., to support instancing multiple render-
    // items), you would need to have a structured buffer for each render-item,
    // and allocate each buffer with enough room for the maximum number of 
    // instances you would ever draw.
    // This sounds like a lot, but it is actually no more than the amount of 
    // per-object constant data we would need if we were not using instancing.
    // For example, if we were drawing 1000 objects without instancing, we 
    // would create a constant buffer with enough room for a 1000 objects.
    // With instancing, we would just create a structured buffer large enough 
    // to store the instance data for 1000 instances.  
    std::vector<std::unique_ptr<UploadBuffer<InstanceData>>> InstanceBuffer;

    // We cannot update a dynamic vertex buffer until the GPU is done processing
    // the commands that reference it.  So each frame needs their own.
    std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

    // Fence value to mark commands up to this fence point. This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};

