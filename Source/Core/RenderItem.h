//*******************************************************************
// RenderItem.h:
//
// Lightweight structure stores parameters to draw a shape. This will
// vary from app-to-app.
//*******************************************************************

#pragma once

#include "DXCore.h"
#include "UploadBuffer.h"
#include "FrameResource.h"

struct RenderItem
{
	// Render Item: The set of data needed to submit a full draw call to
	// the rendering pipeline.
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need 
	// to update the constant buffer. Because we have an object 
	// cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource. Thus, when we modify obect data we 
	// should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource 
	// gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB 
	// for this rendering item.
	UINT ObjCBIndex = -1;

	// Material associated with this render-item. Note that multiple 
	// render-items can refer to the same Material object.
	Material* Mat = nullptr;

	// Geometry associated with this render-item. Note that multiple
	// render-items can be share the same geometry.
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	DirectX::BoundingBox Bounds;

	// The per-instance data in system memory is stored as part of the render-
	// item structure, as the render-item maintains how many times it should
	// be instanced.
	std::vector<InstanceData> Instances;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT InstanceCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	int layerID = 0;
	UINT instanceBufferID = 0;
};
