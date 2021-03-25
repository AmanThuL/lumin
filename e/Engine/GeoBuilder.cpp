//*******************************************************************
// GeoBuilder.cpp by Frank Luna (C) 2011 All Rights Reserved.
//*******************************************************************

#include "GeoBuilder.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

void GeoBuilder::CreateWaves(int m, int n, float dx, float dt, float speed, float damping)
{
    mWaves = std::make_unique<Waves>(m, n, dx, dt, speed, damping);
}

// ------------------------------------------------------------------
// Extract the vertex elements from the MeshData grid. Turn the flat
// grid into a surface representing hills. Generate a color for each
// vertex based on the vertex altitute (y-coordinate).
// ------------------------------------------------------------------
void GeoBuilder::BuildLandGeometry(ComPtr<ID3D12Device> pDevice, ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName)
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height 
    // function to each vertex. In addition, color the vertices based on their 
    // height so we have sandy looking beaches, grassy low hills, and snow 
    // mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
        vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = geoName;

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = DXUtil::CreateDefaultBuffer(pDevice.Get(),
        pCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = DXUtil::CreateDefaultBuffer(pDevice.Get(),
        pCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries[geoName] = std::move(geo);
}

// ------------------------------------------------------------------
// Build waves geometry buffers, caches the necessary drawing quantities,
// and draws the objects.
// ------------------------------------------------------------------
void GeoBuilder::BuildWavesGeometry(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName)
{
    if (mWaves == nullptr)
        assert("Waves object has not been initialized!");

    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
    assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            indices[k] = i * n + j;
            indices[k + 1] = i * n + j + 1;
            indices[k + 2] = (i + 1) * n + j;

            indices[k + 3] = (i + 1) * n + j;
            indices[k + 4] = i * n + j + 1;
            indices[k + 5] = (i + 1) * n + j + 1;

            k += 6; // next quad
        }
    }

    UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = geoName;

    // Set dynamically.
    geo->VertexBufferCPU = nullptr;
    geo->VertexBufferGPU = nullptr;

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->IndexBufferGPU = DXUtil::CreateDefaultBuffer(pDevice.Get(),
        pCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries[geoName] = std::move(geo);
}

void GeoBuilder::BuildShapeGeometry(Microsoft::WRL::ComPtr<ID3D12Device> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList, std::string geoName)
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //
    // 
    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    UINT boxVertexOffset = 0;
    UINT sphereVertexOffset = boxVertexOffset + (UINT)box.Vertices.size();
    UINT gridVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT cylinderVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    UINT boxIndexOffset = 0;
    UINT sphereIndexOffset = boxIndexOffset + (UINT)box.Indices32.size();
    UINT gridIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT cylinderIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();


    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    //
    // Extract the vertex elements we are interested in and pack the
    // vertices of all the meshes into one vertex buffer.
    //

    auto totalVertexCount = box.Vertices.size() + sphere.Vertices.size() + 
        grid.Vertices.size() + cylinder.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    // Bounding Boxes computations
    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    UINT k = 0;
    // Get box's bounding box
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        auto& p = box.Vertices[i].Position;
        vertices[k].Pos = p;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;

        XMVECTOR P = XMLoadFloat3(&p);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }
    BoundingBox boxBounds;
    XMStoreFloat3(&boxBounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&boxBounds.Extents, 0.5f * (vMax - vMin));
    boxSubmesh.Bounds = boxBounds;

    // Get sphere's bounding box
    vMin = XMLoadFloat3(&vMinf3);
    vMax = XMLoadFloat3(&vMaxf3);

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        auto& p = sphere.Vertices[i].Position;
        vertices[k].Pos = p;
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].TexC = sphere.Vertices[i].TexC;

        XMVECTOR P = XMLoadFloat3(&p);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }
    BoundingBox sphereBounds;
    XMStoreFloat3(&sphereBounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&sphereBounds.Extents, 0.5f * (vMax - vMin));
    sphereSubmesh.Bounds = sphereBounds;

    // Get grid's bounding box
    vMin = XMLoadFloat3(&vMinf3);
    vMax = XMLoadFloat3(&vMaxf3);

    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[k].Pos = p;
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].TexC = grid.Vertices[i].TexC;

        XMVECTOR P = XMLoadFloat3(&p);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }
    BoundingBox gridBounds;
    XMStoreFloat3(&gridBounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&gridBounds.Extents, 0.5f * (vMax - vMin));
    gridSubmesh.Bounds = gridBounds;

    // Get cylinder's bounding box
    vMin = XMLoadFloat3(&vMinf3);
    vMax = XMLoadFloat3(&vMaxf3);

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        auto& p = cylinder.Vertices[i].Position;
        vertices[k].Pos = p;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].TexC = cylinder.Vertices[i].TexC;

        XMVECTOR P = XMLoadFloat3(&p);

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }
    BoundingBox cylinderBounds;
    XMStoreFloat3(&cylinderBounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&cylinderBounds.Extents, 0.5f * (vMax - vMin));
    cylinderSubmesh.Bounds = cylinderBounds;


    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));


    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = geoName;

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = DXUtil::CreateDefaultBuffer(pDevice.Get(),
        pCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = DXUtil::CreateDefaultBuffer(pDevice.Get(),
        pCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;

    mGeometries[geoName] = std::move(geo);
}

// ------------------------------------------------------------------
// Return a value from function y = f(x,z) and apply it to each grid
// point.
// ------------------------------------------------------------------
float GeoBuilder::GetHillsHeight(float x, float z)const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

// ------------------------------------------------------------------
// Return the normal value of each grid point.
// ------------------------------------------------------------------
XMFLOAT3 GeoBuilder::GetHillsNormal(float x, float z)const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
        1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}

