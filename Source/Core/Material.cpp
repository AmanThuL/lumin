//*******************************************************************
// Material.cpp
//*******************************************************************
#include "lmpch.h"
#include "Material.h"

using namespace DirectX;

// ========================== Material Class ==========================

Material::Material(const std::string& name) : mName(name) {}

Material::SharedPtr Material::Create(const std::string& name)
{
	Material* pMaterial = new Material(name);
	return SharedPtr(pMaterial);
}

Material::~Material() = default;

MaterialData Material::GetMaterialData()
{
	// Convert MatTransform to XMMATRIX for transpose operation
	XMMATRIX matTransform = XMLoadFloat4x4(&mMatTransform);

	MaterialData matData;
	matData.DiffuseAlbedo = mDiffuseAlbedo;
	matData.FresnelR0 = mFresnelR0;
	matData.Roughness = mRoughness;
	XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
	matData.DiffuseMapIndex = mDiffuseSrvHeapIndex;

	return matData;
}


// ========================== Material Wrapper Class ==========================

void MaterialWrapper::AddMaterial(const Material::SharedPtr& pMaterial)
{
	assert(pMaterial);

	// Check if key not found and not previously added
	if (auto it = mMaterialsTable.find(pMaterial->GetName()); it == mMaterialsTable.end())
		mMaterialsTable[pMaterial->GetName()] = pMaterial;
}

Material::SharedPtr MaterialWrapper::GetMaterial(const std::string& name)
{
	if (auto it = mMaterialsTable.find(name); it == mMaterialsTable.end())
		assert("Material not found!");

	return mMaterialsTable[name];
}
