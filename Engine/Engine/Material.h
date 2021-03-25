//*******************************************************************
// Material.h:
//
// Wrapper class for defining properties of each unique material and
// storing them.
//*******************************************************************

#pragma once

#include "Common/DXUtil.h"

struct MaterialData
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT MaterialPad0;
	UINT MaterialPad1;
	UINT MaterialPad2;
};

class Material : public std::enable_shared_from_this<Material>
{
public:
	using SharedPtr = std::shared_ptr<Material>;
	using SharedConstPtr = std::shared_ptr<const Material>;

	/** Create a new material.
		\param[in] name The material name
	*/
	static SharedPtr Create(const std::string& name);

	~Material();

	/* Set the material name.*/
	void SetName(const std::string& name) { mName = name; }

	/* Get the material name.*/
	const std::string& GetName() const { return mName; }


	/* Set the index into constant buffer corresponding to this material.*/
	void SetMatCBIndex(int index) { mMatCBIndex = index; }

	/* Get the index into constant buffer corresponding to this material.*/
	int GetMatCBIndex() const { return mMatCBIndex; }

	/* Set the index into SRV heap for diffuse texture.*/
	void SetDiffuseSrvHeapIndex(int index) { mDiffuseSrvHeapIndex = index; }

	/* Get the index into SRV heap for diffuse texture.*/
	int GetDiffuseSrvHeapIndex() const { return mDiffuseSrvHeapIndex; }

	/* Set the index into SRV heap for diffuse texture.*/
	void SetNormalSrvHeapIndex(int index) { mNormalSrvHeapIndex = index; }

	/* Get the index into SRV heap for diffuse texture.*/
	int GetNormalSrvHeapIndex() const { return mNormalSrvHeapIndex; }


	/* Set the base color.*/
	void SetDiffuseAlbedo(const DirectX::XMFLOAT4& color) { mDiffuseAlbedo = color; }

	/* Get the base color.*/
	const DirectX::XMFLOAT4& GetDiffuseAlbedo() const { return mDiffuseAlbedo; }

	/* Set the Fresnel coefficient.*/
	void SetFresnel(const DirectX::XMFLOAT3 fresnelR0) { mFresnelR0 = fresnelR0; }

	/* Get the Fresnel coefficient.*/
	const DirectX::XMFLOAT3& GetFresnel() const { return mFresnelR0; }

	/* Set the roughness [0.0f, 1.0f].*/
	void SetRoughness(const float roughness) { mRoughness = roughness; }

	/* Get the Fresnel coefficient.*/
	const float& GetRoughness() const { return mRoughness; }

	/* Set the material transform matrix.*/
	void SetTransform(const DirectX::XMFLOAT4X4 transform) { mMatTransform = transform; }

	/* Get the material transform matrix.*/
    DirectX::XMFLOAT4X4 GetTransform() { return mMatTransform; }

	/* Set a value in material transform matrix.*/
	void SetTransformRowCol(const int& row, const int& col, float value) { mMatTransform(row, col) = value; }


	/* Set dirty flag.*/
	void SetNumFramesDirty(const int dirty) { mNumFramesDirty = dirty; }

	/* Get dirty flag.*/
	const int& GetNumFramesDirty() const { return mNumFramesDirty; }

	/* Decrement dirty flag by 1.*/
	void DecrementNumFramesDirty() { mNumFramesDirty--; }

	/* Get material data for constant buffer.*/
	MaterialData GetMaterialData();


private:
	Material(const std::string& name);

	// Unique material name for lookup.
	std::string mName;

	// Index into constant buffer corresponding to this material.
	int mMatCBIndex = -1;

	// Index into SRV heap for diffuse texture.
	int mDiffuseSrvHeapIndex = -1;

	// Index into SRV heap for normal texture.
	int mNormalSrvHeapIndex = -1;

	// Dirty flag indicating the material has changed and we need to update the
	// constant buffer. Because we have a material constant buffer for each 
	// FrameResource, we have to apply the update to each FrameResource. Thus, 
	// when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the
	// update.
	int mNumFramesDirty = gNumFrameResources;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4 mDiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };	// Diffuse/albedo
	DirectX::XMFLOAT3 mFresnelR0 = { 0.01f, 0.01f, 0.01f };
	float mRoughness = .25f;  // 0 = Perfectly smooth, 1 = Roughest surface
	DirectX::XMFLOAT4X4 mMatTransform = MathHelper::Identity4x4();
};

class MaterialWrapper
{
public:
	MaterialWrapper() = default;

	void AddMaterial(const Material::SharedPtr& pMaterial);
	
	std::unordered_map<std::string, Material::SharedPtr> GetTable() const { return mMaterialsTable; }
	Material::SharedPtr GetMaterial(const std::string& name);
	UINT GetSize() { return (UINT)mMaterialsTable.size(); }

private:
	std::unordered_map<std::string, Material::SharedPtr> mMaterialsTable;
};