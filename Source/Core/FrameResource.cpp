//*******************************************************************
// FrameResource.cpp
//*******************************************************************
#include "lmpch.h"
#include "FrameResource.h"

// Constructor
FrameResource::FrameResource(ID3D12Device* device, UINT passCount, std::vector<UINT> maxInstanceCounts, UINT materialCount, UINT waveVertCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);

	// InstanceBuffer is not a constant buffer, so we specify false for the
	// last parameter.
	for (int i = 0; i < maxInstanceCounts.size(); i++)
	{
		InstanceBuffer.push_back(std::make_unique<UploadBuffer<InstanceData>>(device, maxInstanceCounts[i], false));
	}

	if (waveVertCount != 0)
		WavesVB = std::make_unique<UploadBuffer<Vertex>>(device, waveVertCount, false);
}

FrameResource::~FrameResource()
{
}
