//*******************************************************************
// DescriptorHeapWrapper.h:
//
// Wrapper class for managing descriptor handles and heaps.
//*******************************************************************
#pragma once

#include "common/DXUtil.h"

enum RESOURCE_VIEW_TYPE
{
	RESOURCE_VIEW_TYPE_CBV,
	RESOURCE_VIEW_TYPE_SRV,
	RESOURCE_VIEW_TYPE_UAV,
	RESOURCE_VIEW_TYPE_RTV,
	RESOURCE_VIEW_TYPE_DSV,
};

class DescriptorHeapWrapper
{
public:
	DescriptorHeapWrapper();

	HRESULT Create(
		Microsoft::WRL::ComPtr<ID3D12Device> pDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE Type,
		UINT NumDescriptors,
		bool bShaderVisible = false);

	ID3D12DescriptorHeap* GetHeapPtr();

	UINT GetLastDescIndex();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index);

	void CreateSrvDescriptor(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, ID3D12Resource* resource, const D3D12_SRV_DIMENSION& dimension);

private:
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDH;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	UINT descriptorSize;
	UINT lastDescIndex;
};