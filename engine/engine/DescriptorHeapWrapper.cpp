//*******************************************************************
// DescriptorHeapWrapper.cpp
//*******************************************************************

#include "DescriptorHeapWrapper.h"

using Microsoft::WRL::ComPtr;

DescriptorHeapWrapper::DescriptorHeapWrapper()
{
	memset(this, 0, sizeof(*this));
}

HRESULT DescriptorHeapWrapper::Create(ComPtr<ID3D12Device> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool bShaderVisible)
{
	this->heapDesc.Type = heapType;
	this->heapDesc.NumDescriptors = numDescriptors;
	this->heapDesc.Flags = (bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	this->lastDescIndex = 0;

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pDH)));

	cpuHandle = pDH->GetCPUDescriptorHandleForHeapStart();
	gpuHandle = pDH->GetGPUDescriptorHandleForHeapStart();

	// Get the increment size of a descriptor in this heap type. This is 
	// hardware specific, so we have to query this information.
	descriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapDesc.Type);

	return S_OK;
}

ID3D12DescriptorHeap* DescriptorHeapWrapper::GetHeapPtr()
{
	return pDH.Get();
}

UINT DescriptorHeapWrapper::GetLastDescIndex()
{
	return this->lastDescIndex;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeapWrapper::GetCPUHandle(UINT index)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE offsettedCPUHandle(pDH->GetCPUDescriptorHandleForHeapStart(), index, descriptorSize);
	return offsettedCPUHandle;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeapWrapper::GetGPUHandle(UINT index)
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE offsettedGPUHandle(pDH->GetGPUDescriptorHandleForHeapStart(), index, descriptorSize);
	return offsettedGPUHandle;
}

void DescriptorHeapWrapper::CreateSrvDescriptor(ComPtr<ID3D12Device> pDevice, ID3D12Resource* resource, const D3D12_SRV_DIMENSION& dimension)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	//switch (dimension)
	//{
	//case D3D12_SRV_DIMENSION_TEXTURECUBE:
	//	break;
	//default:
	//	break;
	//}

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = resource->GetDesc().Format;
	srvDesc.ViewDimension = dimension;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	pDevice->CreateShaderResourceView(resource, &srvDesc, GetCPUHandle(lastDescIndex));

	// Increment descriptor handle
	lastDescIndex++;
}

