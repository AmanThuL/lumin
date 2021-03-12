//*******************************************************************
// LandAndWavesApp.cpp
//*******************************************************************

#include "Game.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

Game::Game(HINSTANCE hInstance)
	: DXCore(hInstance)	 // The application's handle
{
#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully. Feel free to printf() here.\n");
#endif
}

// ------------------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// ------------------------------------------------------------------
Game::~Game()
{
    // ImGui clean up
    GUI::ShutDown();

	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

// ------------------------------------------------------------------
// Called once per program, after DirectX and the window are 
// initialized but before the game loop.
// ------------------------------------------------------------------
bool Game::Initialize()
{
    if (!DXCore::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCamera.SetPosition(mDefaultCamPos);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();

    // Build scene implicit geometries
    mGeoBuilder = make_unique<GeoBuilder>();
    mGeoBuilder->CreateWaves(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
    mGeoBuilder->BuildShapeGeometry(md3dDevice, mCommandList, "shapeGeo");

    //BuildLandGeometry();
    //BuildWavesGeometry();
    //BuildBoxGeometry();

    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}

// ------------------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size. For 
// instance, updating our projection matrix's aspect ratio.
// ------------------------------------------------------------------
void Game::OnResize()
{
    DXCore::OnResize();

    // When the window is resized, delegate the work to the Camera class.
    mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

    BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());
}

// ------------------------------------------------------------------
// Called every frame and should be used to update the 3D application 
// over time.
// E.g. Perform animations, move the camera, do collision detection, 
// check for user input, and etc.).
// ------------------------------------------------------------------
void Game::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame
    // resource. If not, wait until the GPU has completed commands up to
    // this fence point.
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    AnimateMaterials(gt);
    UpdateInstanceData(gt);
    UpdateMaterialBuffer(gt);
    UpdateMainPassCB(gt);

    //UpdateWaves(gt);

    //cout << "Current Camera Position: " 
    //     << mCamera.GetPosition3f().x << " " 
    //     << mCamera.GetPosition3f().y << " "
    //     << mCamera.GetPosition3f().z << endl;
}

// ------------------------------------------------------------------
// Invoked every frame and is where rendering commands are issued to 
// draw the current frame to the back buffer.
// ------------------------------------------------------------------
void Game::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording. We can only reset 
    // when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue
    // via ExecuteCommandList. Reusing the command list reuses memory.
    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    // Set the viewport and scissor rect.  This needs to be reset whenever the 
    // command list is reset.
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // Set the descriptor heaps to the command list.
    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap->GetHeapPtr() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // ======================== Root Signature Update =========================
    // Set the root signature to the command list.
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // Bind all the materials used in this scene. For structured buffers, we
    // can bypass the heap and set as a root descriptor.
    auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
    mCommandList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

    // Bind per-pass constant buffer. We only need to do this once per-pass.
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    // Bind the sky cube map.  For our demos, we just use one "world" cube map 
    // representing the environment from far away, so all objects will use the 
    // same cube map and we only need to set it once per-frame.  
    // If we wanted to use "local" cube maps, we would have to change them 
    // per-object, or dynamically index into an array of cube maps.
    mCommandList->SetGraphicsRootDescriptorTable(3, mCbvSrvUavDescriptorHeap->GetGPUHandle(mSkyTexHeapIndex));

    // Bind all the textures used in this scene. Observe that we only have to
    // specify the first descriptor in the table. The root signature knows how
    // many descriptors are expected in the table.
    mCommandList->SetGraphicsRootDescriptorTable(4, mCbvSrvUavDescriptorHeap->GetGPUHandle(0));
    // ========================================================================
    // Draw render items and set pipeline states
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

    mCommandList->SetPipelineState(mPSOs["sky"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

    mCommandList->SetPipelineState(mPSOs["transparent"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // GUI rendering
    GUI::StartFrame();
    DrawGUI();
    GUI::RenderFrame(mCommandList.Get(), mCbvSrvUavDescriptorHeap.get());

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;


    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be set 
    // until the GPU finishes processing all the commands prior to this
    // Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);

    // Note that GPU could still be working on commands from previous frames, 
    // but that is okay, because we are not touching any frame resources 
    // associated with those frames.
}

#pragma region Update Methods

// ------------------------------------------------------------------
// Check for keyboard input per frame.
// ------------------------------------------------------------------
void Game::OnKeyboardInput(const GameTimer& gt)
{
    // Handle keyboard input to move the camera
    const float dt = gt.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000)
        mCamera.Walk(10.0f * dt);
    if (GetAsyncKeyState('S') & 0x8000)
        mCamera.Walk(-10.0f * dt);
    if (GetAsyncKeyState('A') & 0x8000)
        mCamera.Strafe(-10.0f * dt);
    if (GetAsyncKeyState('D') & 0x8000)
        mCamera.Strafe(10.0f * dt);
    if (GetAsyncKeyState('Q') & 0x8000)
        mCamera.MoveY(10.0f * dt);
    if (GetAsyncKeyState('E') & 0x8000)
        mCamera.MoveY(-10.0f * dt);

    // Update camera's view matrix
    mCamera.UpdateViewMatrix();

    // Toggle wireframe mode
    if (GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
}

// ------------------------------------------------------------------
// Translate the texture coordinates in the texture plane as a function
// of time.
// ------------------------------------------------------------------
void Game::AnimateMaterials(const GameTimer& gt)
{
    // Scroll the water material texture coordinates.
    auto waterMat = mMaterials["water"].get();

    float& tu = waterMat->MatTransform(3, 0);
    float& tv = waterMat->MatTransform(3, 1);

    tu += 0.1f * gt.DeltaTime();
    tv += 0.02f * gt.DeltaTime();

    if (tu >= 1.0f)
        tu -= 1.0f;

    if (tv >= 1.0f)
        tv -= 1.0f;

    waterMat->MatTransform(3, 0) = tu;
    waterMat->MatTransform(3, 1) = tv;

    // Material has changed, so need to update cbuffer.
    waterMat->NumFramesDirty = gNumFrameResources;
}

// ------------------------------------------------------------------
// Update the per instance data.
// ------------------------------------------------------------------
void Game::UpdateInstanceData(const GameTimer& gt)
{
    totalVisibleInstanceCount = 0;

    XMMATRIX view = mCamera.GetView();
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

    if (mAllRitems.empty())
        return;

    //int rItemIndex = 0;
    for (auto& e : mAllRitems)
    {
        auto currInstanceBuffer = mCurrFrameResource->InstanceBuffer[e->instanceBufferID].get();

        const auto& instanceData = e->Instances;

        int visibleInstanceCount = 0;

        // Disable frustum culling for skybox
        if (e->layerID == (int)RenderLayer::Sky)
            mFrustumCullingEnabled = false;

        for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
        {
            XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
            XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

            XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

            // View space to the object's local space.
            XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

            // Transform the camera frustum from view space to the object's local space.
            BoundingFrustum localSpaceFrustum;
            mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

            // Perform the box/frustum intersection test in local space.
            if ((localSpaceFrustum.Contains(e->Bounds) != DirectX::DISJOINT) || (mFrustumCullingEnabled == false))
            {
                InstanceData data;
                XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
                XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
                data.MaterialIndex = instanceData[i].MaterialIndex;

                // Write the instance data to structured buffer for the visible objects.
                currInstanceBuffer->CopyData(visibleInstanceCount++, data);
            }
        }

        e->InstanceCount = visibleInstanceCount;
        mFrustumCullingEnabled = true;

        totalVisibleInstanceCount += visibleInstanceCount;
    }
}

// ------------------------------------------------------------------
// The material data is copied to a subregion of the constant buffer
// whenever it is changed ("dirty) so that the GPU material constant
// buffer data is kept up to date with the system memory material data.
// ------------------------------------------------------------------
void Game::UpdateMaterialBuffer(const GameTimer& gt)
{
    auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialData matData;
            matData.DiffuseAlbedo = mat->DiffuseAlbedo;
            matData.FresnelR0 = mat->FresnelR0;
            matData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}

// ------------------------------------------------------------------
// Update the per pass constant buffer only once per rendering pass.
// ------------------------------------------------------------------
void Game::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = mCamera.GetView();
    XMMATRIX proj = mCamera.GetProj();

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    // Main pass constant buffer
    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    mMainPassCB.EyePosW = mCamera.GetPosition3f();
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;

    // Time
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();

    // Lights
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
    mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

// ------------------------------------------------------------------
// Run the wave simulation and update the vertex buffer.
// ------------------------------------------------------------------
void Game::UpdateWaves(const GameTimer& gt)
{
    // Every quarter second, generate a random wave.
    static float t_base = 0.0f;
    if ((gt.TotalTime() - t_base) >= 0.25f)
    {
        t_base += 0.25f;

        int i = MathHelper::Rand(4, mGeoBuilder->GetWaves()->RowCount() - 5);
        int j = MathHelper::Rand(4, mGeoBuilder->GetWaves()->ColumnCount() - 5);

        float r = MathHelper::RandF(0.2f, 0.5f);

        mGeoBuilder->GetWaves()->Disturb(i, j, r);
    }

    // Update the wave simulation.
    mGeoBuilder->GetWaves()->Update(gt.DeltaTime());

    // Update the wave vertex buffer with the new solution.
    auto currWavesVB = mCurrFrameResource->WavesVB.get();
    for (int i = 0; i < mGeoBuilder->GetWaves()->VertexCount(); ++i)
    {
        Vertex v;

        v.Pos = mGeoBuilder->GetWaves()->Position(i);
        v.Normal = mGeoBuilder->GetWaves()->Normal(i);

        // Derive tex-coords from position by 
        // mapping [-w/2,w/2] --> [0,1]
        v.TexC.x = 0.5f + v.Pos.x / mGeoBuilder->GetWaves()->Width();
        v.TexC.y = 0.5f - v.Pos.z / mGeoBuilder->GetWaves()->Depth();

        currWavesVB->CopyData(i, v);
    }

    // Set the dynamic VB of the wave renderitem to the current frame VB.
    mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

#pragma endregion


#pragma region Build Methods

// ------------------------------------------------------------------
// Create the texture from file and store all of the unique textures 
// in an unordered map at initialization time.
// ------------------------------------------------------------------
void Game::LoadTextures()
{
    mTextures = make_unique<TextureWrapper>();
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "bricksTex",  L"bricks.dds");
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "waterTex",   L"water1.dds");
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "iceTex",     L"ice.dds");
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "grassTex",   L"grass.dds");
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "whiteTex",   L"white1x1.dds");

    // Crate textures
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "crate01Tex", L"WoodCrate01.dds");
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "crate02Tex", L"WoodCrate02.dds");

    // Skybox texture
    mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, "skyCubeMap", L"skyboxes/room.dds");
}

// ------------------------------------------------------------------
// Create a root signature that defines what resources the application 
// will bind to the rendering pipeline before a draw call can be 
// executed and where those resources get mapped to shader input 
// registers.
// ------------------------------------------------------------------
void Game::BuildRootSignature()
{
    // - Shader programs typically require resources as input (constant 
    // buffers, textures, samplers). The root signature defines what resources 
    // need to be bound to the pipeline before issuing a draw call and how 
    // those resources get mapped to shader input registers.
    // - If we think of the shader programs as a function, and the input 
    // resources as function parameters, then the root signature can be thought
    // of as defining the function signature.
    // - A root signature is defined by an array of root parameters.
    // - A root parameter can be one of three types:
    //		+ Descriptor table
    //		+ Root descriptor
    //		+ Root constant

    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 99, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

    // Create root CBVs.
    // Performance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsShaderResourceView(0, 1);
    slotRootParameter[1].InitAsShaderResourceView(1, 1);
    slotRootParameter[2].InitAsConstantBufferView(0);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);


    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // Create a root signature with a single slot which points to a descriptor
    // range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    // Error checking
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

// ------------------------------------------------------------------
// Create the descriptor heaps and populate them with actual descriptors.
// ------------------------------------------------------------------
void Game::BuildDescriptorHeaps()
{
    //
    // Create the SRV heap.
    //
    mCbvSrvUavDescriptorHeap = make_unique<DescriptorHeapWrapper>();
    mCbvSrvUavDescriptorHeap->Create(md3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 99, true);

    //
    // Fill out the heap with actual descriptors.
    //
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("bricksTex").Get(),  D3D12_SRV_DIMENSION_TEXTURE2D);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("waterTex").Get(),   D3D12_SRV_DIMENSION_TEXTURE2D);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("crate01Tex").Get(), D3D12_SRV_DIMENSION_TEXTURE2D);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("crate02Tex").Get(), D3D12_SRV_DIMENSION_TEXTURE2D);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("iceTex").Get(),     D3D12_SRV_DIMENSION_TEXTURE2D);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("grassTex").Get(),   D3D12_SRV_DIMENSION_TEXTURE2D);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("whiteTex").Get(),   D3D12_SRV_DIMENSION_TEXTURE2D);

    mSkyTexHeapIndex = mCbvSrvUavDescriptorHeap->GetLastDescIndex();
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("skyCubeMap").Get(), D3D12_SRV_DIMENSION_TEXTURECUBE);

    GUI::SetupRenderer(md3dDevice.Get(), mCbvSrvUavDescriptorHeap.get());
}

// ------------------------------------------------------------------
// Compile shader programs to a portable bycode, and provide Direct3D
// with a description of the vertex structure in the form of an input 
// layout description.
// ------------------------------------------------------------------
void Game::BuildShadersAndInputLayout()
{
    const D3D_SHADER_MACRO defines[] =
    {
        //"FOG", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        //"FOG", "1",
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    mShaders["standardVS"] = DXUtil::CompileShader(L"..\\..\\engine\\engine\\shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = DXUtil::CompileShader(L"..\\..\\engine\\engine\\shaders\\Default.hlsl", defines, "PS", "ps_5_1");
    mShaders["alphaTestedPS"] = DXUtil::CompileShader(L"..\\..\\engine\\engine\\shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mShaders["skyVS"] = DXUtil::CompileShader(L"..\\..\\engine\\engine\\shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skyPS"] = DXUtil::CompileShader(L"..\\..\\engine\\engine\\shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

// ------------------------------------------------------------------
// Build an aggregate pipeline state object to validate all the state 
// is compatible and the driver can generate all the code up front to 
// program the hardware state.
// ------------------------------------------------------------------
void Game::BuildPSOs()
{
    // In the new Direct3D 12 model, the driver can generate all the code 
    // needed to program the pipeline state at initialization time because we 
    // specify the majority of pipeline state as an aggregate.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // PSO for opaque objects.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;

    // Create an ID3D12PipelineState object using the descriptor we filled out
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for opaque wireframe objects.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));


    //
    // PSO for sky.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

    // The camera is inside the sky sphere, so just turn off culling.
    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Make sure the depth function is LESS_EQUAL and not just LESS.  
    // Otherwise, the normalized depth values at z = 1 (NDC) will 
    // fail the depth test if the depth buffer was cleared to 1.
    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    skyPsoDesc.pRootSignature = mRootSignature.Get();
    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
        mShaders["skyVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
        mShaders["skyPS"]->GetBufferSize()
    };
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));


    //
    // PSO for transparent objects
    //
    // Start from non-blended PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

    //
    // PSO for alpha tested objects
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
    alphaTestedPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
        mShaders["alphaTestedPS"]->GetBufferSize()
    };
    alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));
}

// ------------------------------------------------------------------
// Build a circular array of the resources the CPU needs to modify 
// each frame to keep both CPU and GPU busy.
// ------------------------------------------------------------------
void Game::BuildFrameResources()
{
    // Having multiple frame resources do not prevent any waiting, but it helps
    // us keep the GPU fed. While the GPU is processing commands from frame n, 
    // it allows the CPU to continue on to build and submit commands for frames
    // n+1 and n+2. This helps keep the command queue nonempty so that the GPU
    // always has work to do.
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, mInstanceCounts, (UINT)mMaterials.size(), mGeoBuilder->GetWaves()->VertexCount()));
    }
}

// ------------------------------------------------------------------
// Define the properties of each unique material and put them in a table.
// ------------------------------------------------------------------
void Game::BuildMaterials()
{
    auto bricks = std::make_unique<Material>();
    bricks->Name = "bricks";
    bricks->MatCBIndex = 0;
    bricks->DiffuseSrvHeapIndex = 0;
    bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks->Roughness = 0.1f;

    // This is not a good water material definition, but we do not have all the rendering
    // tools we need (transparency, environment reflection), so we fake it for now.
    auto water = std::make_unique<Material>();
    water->Name = "water";
    water->MatCBIndex = 1;
    water->DiffuseSrvHeapIndex = 1;
    water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
    water->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
    water->Roughness = 0.2f;

    auto crate01 = std::make_unique<Material>();
    crate01->Name = "crate01";
    crate01->MatCBIndex = 2;
    crate01->DiffuseSrvHeapIndex = 2;
    crate01->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    crate01->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    crate01->Roughness = 0.5f;

    auto crate02 = std::make_unique<Material>();
    crate02->Name = "crate02";
    crate02->MatCBIndex = 3;
    crate02->DiffuseSrvHeapIndex = 3;
    crate02->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    crate02->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    crate02->Roughness = 0.5f;

    auto ice = std::make_unique<Material>();
    ice->Name = "ice";
    ice->MatCBIndex = 4;
    ice->DiffuseSrvHeapIndex = 4;
    ice->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f);
    ice->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
    ice->Roughness = 0.1f;

    auto grass = std::make_unique<Material>();
    grass->Name = "grass";
    grass->MatCBIndex = 5;
    grass->DiffuseSrvHeapIndex = 5;
    grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    grass->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    grass->Roughness = 0.2f;

    auto mirror = std::make_unique<Material>();
    mirror->Name = "mirror";
    mirror->MatCBIndex = 6;
    mirror->DiffuseSrvHeapIndex = 6;
    mirror->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f);
    mirror->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
    mirror->Roughness = 0.1f;

    auto sky = std::make_unique<Material>();
    sky->Name = "sky";
    sky->MatCBIndex = 7;
    sky->DiffuseSrvHeapIndex = 7;
    sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    sky->Roughness = 1.0f;

    mMaterials["bricks"] = std::move(bricks);
    mMaterials["water"] = std::move(water);
    mMaterials["crate01"] = std::move(crate01);
    mMaterials["crate02"] = std::move(crate02);
    mMaterials["ice"] = std::move(ice);
    mMaterials["grass"] = std::move(grass);
    mMaterials["mirror"] = std::move(mirror);
    mMaterials["sky"] = std::move(sky);
}

// ------------------------------------------------------------------
// Define and build scene render items. All the render items share the 
// same MeshGeometry, we use the DrawArgs to get the DrawIndexedInstanced 
// parameters to draw a subregion of the vertex/index buffers.
// ------------------------------------------------------------------
void Game::BuildRenderItems()
{
    UINT instanceBufferID = 0;
    UINT instanceCount = 0;

    // 1 - Skybox render item
    auto skyRitem = std::make_unique<RenderItem>();
    skyRitem->World = MathHelper::Identity4x4();
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = 0;
    skyRitem->Mat = mMaterials["sky"].get();
    skyRitem->Geo = mGeoBuilder->GetMeshGeo("shapeGeo");
    skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
    skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
    skyRitem->Bounds = skyRitem->Geo->DrawArgs["sphere"].Bounds;

    // Only one Skybox needed
    instanceCount = 1;
    skyRitem->Instances.resize(instanceCount);
    XMStoreFloat4x4(&skyRitem->Instances[0].World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));

    skyRitem->instanceBufferID = instanceBufferID++;
    mInstanceCounts.push_back(instanceCount);
    totalInstanceCount += instanceCount;
    mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());


    // 2 - Box render item
    auto boxRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
    boxRitem->ObjCBIndex = 2;
    boxRitem->Mat = mMaterials["crate01"].get();
    boxRitem->Geo = mGeoBuilder->GetMeshGeo("shapeGeo");
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->InstanceCount = 0;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    boxRitem->Bounds = boxRitem->Geo->DrawArgs["box"].Bounds;

    // Random
    //std::random_device rd;
    //std::mt19937 mt(rd());
    //std::uniform_real_distribution<double> urd(0, std::nextafter(6, DBL_MAX));

    // Generate instance data for box render item.
    const int n = 5;
    instanceCount = n * n * n;
    boxRitem->Instances.resize(instanceCount);

    float width = 200.0f;
    float height = 200.0f;
    float depth = 200.0f;

    float x = -0.5f * width;
    float y = -0.5f * height;
    float z = -0.5f * depth;
    float dx = width / (n - 1);
    float dy = height / (n - 1);
    float dz = depth / (n - 1);
    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                int index = k * n * n + i * n + j;
                // Position instanced along a 3D grid.
                boxRitem->Instances[index].World = XMFLOAT4X4(
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    x + j * dx, y + i * dy, z + k * dz, 1.0f);

                XMStoreFloat4x4(&boxRitem->Instances[index].TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.0f));
                //boxRitem->Instances[index].MaterialIndex = index % 4 + 2;
                boxRitem->Instances[index].MaterialIndex = index % 7;
            }
        }
    }

    boxRitem->instanceBufferID = instanceBufferID++;
    mInstanceCounts.push_back(instanceCount);
    totalInstanceCount += instanceCount;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());

    //// Water render item
    //auto wavesRitem = std::make_unique<RenderItem>();
    //wavesRitem->World = MathHelper::Identity4x4();
    //XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    //wavesRitem->ObjCBIndex = 0;
    //wavesRitem->Mat = mMaterials["water"].get();
    //wavesRitem->Geo = mGeometries["waterGeo"].get();
    //wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
    //wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    //wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    //mWavesRitem = wavesRitem.get();

    //mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

    //// Grid render item
    //auto gridRitem = std::make_unique<RenderItem>();
    //gridRitem->World = MathHelper::Identity4x4();
    //XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    //gridRitem->ObjCBIndex = 1;
    //gridRitem->Mat = mMaterials["grass"].get();
    //gridRitem->Geo = mGeometries["landGeo"].get();
    //gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    //gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    //gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    //mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

    //mAllRitems.push_back(std::move(wavesRitem));
    //mAllRitems.push_back(std::move(gridRitem));
    mAllRitems.push_back(std::move(boxRitem));
    mAllRitems.push_back(std::move(skyRitem));
}

#pragma endregion


// ------------------------------------------------------------------
// Draw stored render items. Invoked in the main Draw call.
// ------------------------------------------------------------------
void Game::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    // For each render item...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];
        
        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // Set the instance buffer to use for this render-item.
        // For structured buffers, we can bypass the heap and set as a root 
        // descriptor.
        auto instanceBuffer = mCurrFrameResource->InstanceBuffer[ri->instanceBufferID]->Resource();
        mCommandList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress());

        cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

// ------------------------------------------------------------------
// Draw customized GUI windows using ImGui framework.
// ------------------------------------------------------------------
void Game::DrawGUI()
{
    const float PAD = 10.0f;
    static int corner = 0;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (corner != -1)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("DEBUG", 0, window_flags))
    {
        ImGui::Text("Resolution: %i x %i", mClientWidth, mClientHeight);
        ImGui::Separator();

        ImGui::Text("Frustum Culling: \n");
        ImGui::Text("%i objects visible out of %i", totalVisibleInstanceCount, totalInstanceCount);
        ImGui::Separator();

        if (ImGui::IsMousePosValid())
            ImGui::Text("Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
        else {
            ImGui::Text("Mouse Position: <invalid>");
        }
    }
    ImGui::End();
}

// ------------------------------------------------------------------
// Define static samplers (2032 max).
// ------------------------------------------------------------------
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Game::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  
    // So just define them all up front and keep them available as part of the 
    // root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0,                                // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,   // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,    // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2,                                // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4,                                // shaderRegister
        D3D12_FILTER_ANISOTROPIC,         // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5,                                 // shaderRegister
        D3D12_FILTER_ANISOTROPIC,          // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}


#pragma region Mouse Input

// ------------------------------------------------------------------
// Helper method for mouse clicking.  We get this information from the 
// OS-level messages anyway, so these helpers have been created to 
// provide basic mouse input if you want it.
// ------------------------------------------------------------------
void Game::OnMouseDown(WPARAM btnState, int x, int y)
{
    // Add any custom code here...

    // Save the previous mouse position, so we have it for the future
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    // Caputure the mouse so we keep getting mouse move events even if the 
    // mouse leaves the window. We'll be releasing the capture once a mouse 
    // button is released
    SetCapture(mhMainWnd);
}

// ------------------------------------------------------------------
// Helper method for mouse release
// ------------------------------------------------------------------
void Game::OnMouseUp(WPARAM btnState, int x, int y)
{
    // Add any custom code here...

    // We don't care about the tracking the cursor outside the window anymore 
    // (we're not dragging if the mouse is up)
    ReleaseCapture();
}

// ------------------------------------------------------------------
// Helper method for mouse movement. We only get this message if the 
// mouse is currently over the window, or if we're currently capturing
// the mouse.
// ------------------------------------------------------------------
void Game::OnMouseMove(WPARAM btnState, int x, int y)
{
    // Rotate the camera's look direction.
    // Hold the right mouse button down and move the mouse to "look" in
    // different directions.
    if ((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

        mCamera.Pitch(dy);
        mCamera.RotateY(dx);
    }

    // Save the previous mouse position, so we have it for the future
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

// ------------------------------------------------------------------
// Helper method for mouse wheel scrolling.  
// WheelDelta may be positive or negative, depending on the direction 
// of the scroll
// ------------------------------------------------------------------
void Game::OnMouseWheel(float wheelDelta, int x, int y)
{
    // Add any custom code here...
}

#pragma endregion