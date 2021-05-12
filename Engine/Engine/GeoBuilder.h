//*******************************************************************
// GeoBuilder.h:
//
// Wrapper class for building implicit geometries such as land, waves,
// primitive shapes, and model from .txt files.
//*******************************************************************

#pragma once

#include "Common/GameTimer.h"
#include "FrameResource.h"

// Waves Class
// Performs the calculations for the wave simulation. After the simulation has 
// been updated, the client must copy the current solution into vertex buffers 
// for rendering. This class only does the calculations, it does not do any 
// drawing.
class Waves
{
public:
	Waves(int m, int n, float dx, float dt, float speed, float damping)
	{
		mNumRows = m;
		mNumCols = n;

		mVertexCount = m * n;
		mTriangleCount = (m - 1) * (n - 1) * 2;

		mTimeStep = dt;
		mSpatialStep = dx;

		float d = damping * dt + 2.0f;
		float e = (speed * speed) * (dt * dt) / (dx * dx);
		mK1 = (damping * dt - 2.0f) / d;
		mK2 = (4.0f - 8.0f * e) / d;
		mK3 = (2.0f * e) / d;

		mPrevSolution.resize(m * n);
		mCurrSolution.resize(m * n);
		mNormals.resize(m * n);
		mTangentX.resize(m * n);

		// Generate grid vertices in system memory.

		float halfWidth = (n - 1) * dx * 0.5f;
		float halfDepth = (m - 1) * dx * 0.5f;
		for (int i = 0; i < m; ++i)
		{
			float z = halfDepth - i * dx;
			for (int j = 0; j < n; ++j)
			{
				float x = -halfWidth + j * dx;

				mPrevSolution[i * n + j] = DirectX::XMFLOAT3(x, 0.0f, z);
				mCurrSolution[i * n + j] = DirectX::XMFLOAT3(x, 0.0f, z);
				mNormals[i * n + j] = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
				mTangentX[i * n + j] = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
			}
		}
	}

	Waves(const Waves& rhs) = delete;
	Waves& operator=(const Waves& rhs) = delete;
	~Waves() {}

	int RowCount()const { return mNumRows; }
	int ColumnCount()const { return mNumCols; }
	int VertexCount()const { return mVertexCount; }
	int TriangleCount()const { return mTriangleCount; }
	float Width()const { return mNumCols * mSpatialStep; }
	float Depth()const { return mNumRows * mSpatialStep; }

	// Returns the solution at the ith grid point.
	const DirectX::XMFLOAT3& Position(int i)const { return mCurrSolution[i]; }

	// Returns the solution normal at the ith grid point.
	const DirectX::XMFLOAT3& Normal(int i)const { return mNormals[i]; }

	// Returns the unit tangent vector at the ith grid point in the local x-axis direction.
	const DirectX::XMFLOAT3& TangentX(int i)const { return mTangentX[i]; }

	void Update(float dt)
	{
		static float t = 0;

		// Accumulate time.
		t += dt;

		// Only update the simulation at the specified time step.
		if (t >= mTimeStep)
		{
			// Only update interior points; we use zero boundary conditions.
			concurrency::parallel_for(1, mNumRows - 1, [this](int i)
				//for(int i = 1; i < mNumRows-1; ++i)
				{
					for (int j = 1; j < mNumCols - 1; ++j)
					{
						// After this update we will be discarding the old previous
						// buffer, so overwrite that buffer with the new update.
						// Note how we can do this inplace (read/write to same element) 
						// because we won't need prev_ij again and the assignment happens last.

						// Note j indexes x and i indexes z: h(x_j, z_i, t_k)
						// Moreover, our +z axis goes "down"; this is just to 
						// keep consistent with our row indices going down.

						mPrevSolution[i * mNumCols + j].y =
							mK1 * mPrevSolution[i * mNumCols + j].y +
							mK2 * mCurrSolution[i * mNumCols + j].y +
							mK3 * (mCurrSolution[(i + 1) * mNumCols + j].y +
								mCurrSolution[(i - 1) * mNumCols + j].y +
								mCurrSolution[i * mNumCols + j + 1].y +
								mCurrSolution[i * mNumCols + j - 1].y);
					}
				});

			// We just overwrote the previous buffer with the new data, so
			// this data needs to become the current solution and the old
			// current solution becomes the new previous solution.
			std::swap(mPrevSolution, mCurrSolution);

			t = 0.0f; // reset time

			//
			// Compute normals using finite difference scheme.
			//
			concurrency::parallel_for(1, mNumRows - 1, [this](int i)
				//for(int i = 1; i < mNumRows - 1; ++i)
				{
					for (int j = 1; j < mNumCols - 1; ++j)
					{
						float l = mCurrSolution[i * mNumCols + j - 1].y;
						float r = mCurrSolution[i * mNumCols + j + 1].y;
						float t = mCurrSolution[(i - 1) * mNumCols + j].y;
						float b = mCurrSolution[(i + 1) * mNumCols + j].y;
						mNormals[i * mNumCols + j].x = -r + l;
						mNormals[i * mNumCols + j].y = 2.0f * mSpatialStep;
						mNormals[i * mNumCols + j].z = b - t;

						DirectX::XMVECTOR n = DirectX::XMVector3Normalize(XMLoadFloat3(&mNormals[i * mNumCols + j]));
						XMStoreFloat3(&mNormals[i * mNumCols + j], n);

						mTangentX[i * mNumCols + j] = DirectX::XMFLOAT3(2.0f * mSpatialStep, r - l, 0.0f);
						DirectX::XMVECTOR T = DirectX::XMVector3Normalize(XMLoadFloat3(&mTangentX[i * mNumCols + j]));
						XMStoreFloat3(&mTangentX[i * mNumCols + j], T);
					}
				});
		}
	}
	void Disturb(int i, int j, float magnitude)
	{
		// Don't disturb boundaries.
		assert(i > 1 && i < mNumRows - 2);
		assert(j > 1 && j < mNumCols - 2);

		float halfMag = 0.5f * magnitude;

		// Disturb the ijth vertex height and its neighbors.
		mCurrSolution[i * mNumCols + j].y += magnitude;
		mCurrSolution[i * mNumCols + j + 1].y += halfMag;
		mCurrSolution[i * mNumCols + j - 1].y += halfMag;
		mCurrSolution[(i + 1) * mNumCols + j].y += halfMag;
		mCurrSolution[(i - 1) * mNumCols + j].y += halfMag;
	}

private:
	int mNumRows = 0;
	int mNumCols = 0;

	int mVertexCount = 0;
	int mTriangleCount = 0;

	// Simulation constants we can precompute.
	float mK1 = 0.0f;
	float mK2 = 0.0f;
	float mK3 = 0.0f;

	float mTimeStep = 0.0f;
	float mSpatialStep = 0.0f;

	std::vector<DirectX::XMFLOAT3> mPrevSolution;
	std::vector<DirectX::XMFLOAT3> mCurrSolution;
	std::vector<DirectX::XMFLOAT3> mNormals;
	std::vector<DirectX::XMFLOAT3> mTangentX;
};

class GeoBuilder
{
public:
	GeoBuilder() {}

	void CreateWaves(int m, int n, float dx, float dt, float speed, float damping);
	Waves* GetWaves() { return mWaves.get(); }

	MeshGeometry* GetMeshGeo(std::string name) { return mGeometries[name].get(); }

	void BuildLandGeometry(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName);
	void BuildWavesGeometry(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName);
	void BuildShapeGeometry(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName);
	void BuildGeometryFromText(const std::string& path, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName);

protected:
	float GetHillsHeight(float x, float z)const;
	DirectX::XMFLOAT3 GetHillsNormal(float x, float z)const;

private:
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unique_ptr<Waves> mWaves;
};