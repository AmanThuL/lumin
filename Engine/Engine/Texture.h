//*******************************************************************
// Texture.h:
//
// Wrapper class for loading textures from file and storing them during
// initialization time.
//*******************************************************************

#pragma once

#include "Common/DXUtil.h"

class TextureWrapper
{
public:
	TextureWrapper() = default;

	Microsoft::WRL::ComPtr<ID3D12Resource> GetTextureResource(std::string name);

	void CreateDDSTextureFromFile(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, 
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList,
		std::string name, std::wstring fileName);

private:
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
};