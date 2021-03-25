//*******************************************************************
// Texture.cpp
//*******************************************************************

#include "Texture.h"

using Microsoft::WRL::ComPtr;

ComPtr<ID3D12Resource> TextureWrapper::GetTextureResource(std::string name)
{
    if (mTextures.find(name) == mTextures.end())
        assert("Texture not found!");

    return mTextures[name]->Resource;
}

void TextureWrapper::CreateDDSTextureFromFile(ComPtr<ID3D12Device> pDevice, ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string name, std::wstring fileName)
{
    auto newTex = std::make_unique<Texture>();
    newTex->Name = name;
    newTex->Filename = L"../../Engine/Resources/Textures/" + fileName;
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(pDevice.Get(),
        pCommandList.Get(), newTex->Filename.c_str(),
        newTex->Resource, newTex->UploadHeap));

    // Add new texture to texture map
    mTextures[newTex->Name] = std::move(newTex);
}
