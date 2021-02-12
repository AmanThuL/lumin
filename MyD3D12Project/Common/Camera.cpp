//*******************************************************************
// Camera.cpp
//*******************************************************************

#include "Camera.h"

using namespace DirectX;


#pragma region Constructor/Destructor

// ------------------------------------------------------------------
// Constructor - Initialize the properties for the frustum (lens)
// ------------------------------------------------------------------
Camera::Camera()
{
	SetLens(0.25f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
}

// ------------------------------------------------------------------
// Destructor
// ------------------------------------------------------------------
Camera::~Camera()
{
}

#pragma endregion


#pragma region Get/Set Methods for World Camera Position

// ------------------------------------------------------------------
// Convenient get method for the position of the camera.
// ------------------------------------------------------------------
DirectX::XMVECTOR Camera::GetPosition() const
{
	return XMLoadFloat3(&mPosition);
}

// ------------------------------------------------------------------
// Get method for the position of the camera.
// ------------------------------------------------------------------
DirectX::XMFLOAT3 Camera::GetPosition3f() const
{
	return mPosition;
}

// ------------------------------------------------------------------
// Set method for the position of the camera.
// ------------------------------------------------------------------
void Camera::SetPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
	mViewDirty = true;
}

// ------------------------------------------------------------------
// Set method for the position of the camera.
// ------------------------------------------------------------------
void Camera::SetPosition(const DirectX::XMFLOAT3& v)
{
	mPosition = v;
	mViewDirty = true;
}

#pragma endregion


#pragma region Get Methods for Camera Basis Vectors

// ------------------------------------------------------------------
// Convenient get method for the right vector of the camera.
// ------------------------------------------------------------------
DirectX::XMVECTOR Camera::GetRight() const
{
	return XMLoadFloat3(&mRight);
}

// ------------------------------------------------------------------
// Get method for the right vector of the camera.
// ------------------------------------------------------------------
DirectX::XMFLOAT3 Camera::GetRight3f() const
{
	return mRight;
}

// ------------------------------------------------------------------
// Convenient get method for the up vector of the camera.
// ------------------------------------------------------------------
DirectX::XMVECTOR Camera::GetUp() const
{
	return XMLoadFloat3(&mUp);
}

// ------------------------------------------------------------------
// Get method for the up vector of the camera.
// ------------------------------------------------------------------
DirectX::XMFLOAT3 Camera::GetUp3f() const
{
	return mUp;
}

// ------------------------------------------------------------------
// Convenient get method for the look vector of the camera.
// ------------------------------------------------------------------
DirectX::XMVECTOR Camera::GetLook() const
{
	return XMLoadFloat3(&mLook);
}

// ------------------------------------------------------------------
// Get method for the look vector of the camera.
// ------------------------------------------------------------------
DirectX::XMFLOAT3 Camera::GetLook3f() const
{
	return mLook;
}

#pragma endregion


#pragma region Get Methods for Frustum Properties 

// ------------------------------------------------------------------
// Get method for the frustum near-cutting value.
// ------------------------------------------------------------------
float Camera::GetNearZ() const
{
	return mNearZ;
}

// ------------------------------------------------------------------
// Get method for the frustum far-cutting value.
// ------------------------------------------------------------------
float Camera::GetFarZ() const
{
	return mFarZ;
}

// ------------------------------------------------------------------
// Get method for the frustum aspect (width / height).
// ------------------------------------------------------------------
float Camera::GetAspect() const
{
	return mAspect;
}

// ------------------------------------------------------------------
// Get method for the vertical field of view angle in radians.
// ------------------------------------------------------------------
float Camera::GetFovY() const
{
	return mFovY;
}

// ------------------------------------------------------------------
// Get method for the vertical field of view angle in radians.
// ------------------------------------------------------------------
float Camera::GetFovX() const
{
	float halfWidth = 0.5f * GetNearWindowWidth();
	return 2.0f * atan(halfWidth / mNearZ);
}

#pragma endregion


#pragma region Get Methods for Near and Far Plane Dimensions

// ------------------------------------------------------------------
// Return the width of the frustum at the near plane.
// ------------------------------------------------------------------
float Camera::GetNearWindowWidth() const
{
	return mAspect * mNearWindowHeight;
}

// ------------------------------------------------------------------
// Return the height of the frustum at the near plane.
// ------------------------------------------------------------------
float Camera::GetNearWindowHeight() const
{
	return mNearWindowHeight;
}

// ------------------------------------------------------------------
// Return the width of the frustum at the far plane.
// ------------------------------------------------------------------
float Camera::GetFarWindowWidth() const
{
	return mAspect * mFarWindowHeight;
}

// ------------------------------------------------------------------
// Return the height of the frustum at the far plane.
// ------------------------------------------------------------------
float Camera::GetFarWindowHeight() const
{
	return mFarWindowHeight;
}

#pragma endregion


#pragma region Set Method for Frustum

// ------------------------------------------------------------------
// Cache the frustum properties and build the projection matrix.
// ------------------------------------------------------------------
void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	// Cache properties
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	// Build the projection matrix
	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}

#pragma endregion


#pragma region Set Method for Frustum

// ------------------------------------------------------------------
// Compute and store the right, up, and look vectors in view space based
// on XMVECTOR inputs.
// ------------------------------------------------------------------
void Camera::LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);

	mViewDirty = true;
}

// ------------------------------------------------------------------
// Compute and store the right, up, and look vectors in view space based
// on XMFLOAT3 inputs.
// ------------------------------------------------------------------
void Camera::LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);

	mViewDirty = true;
}

#pragma endregion


#pragma region Get Methods for View/Proj Matrices

// ------------------------------------------------------------------
// Convenient get method for the view matrix.
// ------------------------------------------------------------------
DirectX::XMMATRIX Camera::GetView() const
{
	assert(!mViewDirty);
	return XMLoadFloat4x4(&mView);
}

// ------------------------------------------------------------------
// Convenient get method for the projection matrix.
// ------------------------------------------------------------------
DirectX::XMMATRIX Camera::GetProj() const
{
	return XMLoadFloat4x4(&mProj);
}

// ------------------------------------------------------------------
// Get method for the view matrix.
// ------------------------------------------------------------------
DirectX::XMFLOAT4X4 Camera::GetView4x4f() const
{
	assert(!mViewDirty);
	return mView;
}

// ------------------------------------------------------------------
// Get method for the projection matrix.
// ------------------------------------------------------------------
DirectX::XMFLOAT4X4 Camera::GetProj4x4f() const
{
	return mProj;
}

#pragma endregion


#pragma region Methods for Camera Transformation

// ------------------------------------------------------------------
// Move the camera along its right vector to strafe right and left.
// ------------------------------------------------------------------
void Camera::Strafe(float d)
{
	// mPosition += d * mRight
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

	mViewDirty = true;
}

// ------------------------------------------------------------------
// Move the camera along its look vector to move forwards and backwards.
// ------------------------------------------------------------------
void Camera::Walk(float d)
{
	// mPosition += d*mLook
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

	mViewDirty = true;
}

// ------------------------------------------------------------------
// Move the camera along the world's y-axis to move up and down.
// ------------------------------------------------------------------
void Camera::MoveY(float d)
{
	// mPosition += d*mLook
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR worldY = XMLoadFloat3(&mWorldY);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, worldY, p));

	mViewDirty = true;
}

// ------------------------------------------------------------------
// Rotate the camera around its right vector to look up and down.
// ------------------------------------------------------------------
void Camera::Pitch(float angle)
{
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

// ------------------------------------------------------------------
// Rotate the camera around the world's y-axis (assuming the y-axis
// corresponds to the world's "up" direction) vector to look right and
// left.
// ------------------------------------------------------------------
void Camera::RotateY(float angle)
{
	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

#pragma endregion


#pragma region Method to Rebuild the View Matrix

// ------------------------------------------------------------------
// Reorthonormalize the camera's right, up, and look vectors. Plugs the
// camera vectors to compute the view transformation matrix.
// ------------------------------------------------------------------
void Camera::UpdateViewMatrix()
{
	if (mViewDirty)
	{
		// Make sure the camera vectors are mutually orthogonal to each other 
		// and unit length. This is necessary because after several rotations,
		// numerical errors can accumulate and cause these vectors to become 
		// non-orthonormal, which leads to a skewed coordinate system.
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPosition);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		// V = W^{-1} = (RT)^{-1} = T^{-1}R^{-1} = T^{-1}R^{T}
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		mView(0, 0) = mRight.x;
		mView(1, 0) = mRight.y;
		mView(2, 0) = mRight.z;
		mView(3, 0) = x;

		mView(0, 1) = mUp.x;
		mView(1, 1) = mUp.y;
		mView(2, 1) = mUp.z;
		mView(3, 1) = y;

		mView(0, 2) = mLook.x;
		mView(1, 2) = mLook.y;
		mView(2, 2) = mLook.z;
		mView(3, 2) = z;

		mView(0, 3) = 0.0f;
		mView(1, 3) = 0.0f;
		mView(2, 3) = 0.0f;
		mView(3, 3) = 1.0f;

		mViewDirty = false;
	}

}

#pragma endregion
