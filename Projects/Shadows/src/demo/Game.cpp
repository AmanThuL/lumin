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
    // Estimate the scene bounding sphere manually since we know how the scene was constructed.
    // The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
    // the world space origin.  In general, you need to loop over every world space vertex
    // position and compute the bounding sphere.
    mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    mSceneBounds.Radius = sqrtf(30.0f * 30.0f + 30.0f * 30.0f);

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

    // Create the SRV heap.
    mCbvSrvUavDescriptorHeap = make_unique<DescriptorHeapWrapper>();
    mCbvSrvUavDescriptorHeap->Create(md3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 99, true);

    // Create the shadow map.
    mShadowMap = std::make_unique<ShadowMap>(
        md3dDevice.Get(), mCbvSrvUavDescriptorHeap.get(), 2048, 2048);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();

    // Build scene implicit geometries
    mGeoBuilder = make_unique<GeoBuilder>();
    mGeoBuilder->CreateWaves(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
    mGeoBuilder->BuildShapeGeometry(md3dDevice, mCommandList, "shapeGeo");
    mGeoBuilder->BuildGeometryFromText("../../Engine/Resources/Models/car.txt", md3dDevice, mCommandList, "carModel");

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
// Create the RTV and DSV descriptor heaps the application needs.
// ------------------------------------------------------------------
void Game::CreateRtvAndDsvDescriptorHeaps()
{
    // Add +6 RTV for cube render target.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

    // Add +1 DSV for shadow map.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 2;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
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

    //
    // Animate the lights (and hence shadows).
    //

    mLightRotationAngle += 0.1f * gt.DeltaTime();

    XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
        lightDir = XMVector3TransformNormal(lightDir, R);
        XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
    }

    AnimateMaterials(gt);
    UpdateInstanceData(gt);
    UpdateMaterialBuffer(gt);
    UpdateShadowTransform(gt);
    UpdateMainPassCB(gt);
    UpdateShadowPassCB(gt);

    //UpdateWaves(gt);
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
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    // ========================== 1st: Shadow Pass ============================
    // Set the descriptor heaps to the command list.
    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap->GetHeapPtr() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // Set the root signature to the command list.
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // Bind all the materials used in this scene. For structured buffers, we
    // can bypass the heap and set as a root descriptor.
    auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
    mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

    // Bind null SRV for shadow map pass.
    mCommandList->SetGraphicsRootDescriptorTable(3, mCbvSrvUavDescriptorHeap->GetGPUHandle(mNullCubeSrvIndex));

    // Bind all the textures used in this scene. Observe that we only have to
    // specify the first descriptor in the table. The root signature knows how
    // many descriptors are expected in the table.
    mCommandList->SetGraphicsRootDescriptorTable(4, mCbvSrvUavDescriptorHeap->GetGPUHandle(0));

    DrawSceneToShadowMap();

    // ========================== 2nd: Main Pass ============================
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

    // Bind per-pass constant buffer. We only need to do this once per-pass.
    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());

    // Bind the sky cube map.  For our demos, we just use one "world" cube map 
    // representing the environment from far away, so all objects will use the 
    // same cube map and we only need to set it once per-frame.  
    // If we wanted to use "local" cube maps, we would have to change them 
    // per-object, or dynamically index into an array of cube maps.
    mCommandList->SetGraphicsRootDescriptorTable(3, mCbvSrvUavDescriptorHeap->GetGPUHandle(mSkyTexHeapIndex));

    // Draw render items and set pipeline states
    if (mIsWireframe)
        mCommandList->SetPipelineState(mPSOs["opaque_wireframe"].Get());
    else
        mCommandList->SetPipelineState(mPSOs["opaque"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    //mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
    //DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

    mCommandList->SetPipelineState(mPSOs["sky"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

    mCommandList->SetPipelineState(mPSOs["transparent"].Get());
    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


    // GUI rendering
    GUI::StartFrame();
    DrawGUI();
    GUI::RenderFrame(mCommandList.Get(), mCbvSrvUavDescriptorHeap.get());

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
    auto waterMat = mMaterials->GetMaterial("water");

    float& tu = waterMat->GetTransform()(3, 0);
    float& tv = waterMat->GetTransform()(3, 1);

    tu += 0.1f * gt.DeltaTime();
    tv += 0.02f * gt.DeltaTime();

    if (tu >= 1.0f)
        tu -= 1.0f;

    if (tv >= 1.0f)
        tv -= 1.0f;

    waterMat->SetTransformRowCol(3, 0, tu);
    waterMat->SetTransformRowCol(3, 1, tv);

    // Material has changed, so need to update cbuffer.
    waterMat->SetNumFramesDirty(gNumFrameResources);
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

        bool isFCEnabled = mFrustumCullingEnabled;

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
            if ((localSpaceFrustum.Contains(e->Bounds) != DirectX::DISJOINT) || !mFrustumCullingEnabled)
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
        mFrustumCullingEnabled = isFCEnabled;

        totalVisibleInstanceCount += visibleInstanceCount;
    }
}

// ------------------------------------------------------------------
// The material data is copied to a subregion of the constant buffer
// whenever it is changed ("dirty") so that the GPU material constant
// buffer data is kept up to date with the system memory material data.
// ------------------------------------------------------------------
void Game::UpdateMaterialBuffer(const GameTimer& gt)
{
    auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
    for (auto& e : mMaterials->GetTable())
    {
        // Only update the cbuffer data if the constants have changed. If the 
        // cbuffer data changes, it needs to be updated for each FrameResource.
        auto mat = e.second;
        if (mat->GetNumFramesDirty() > 0)
        {
            currMaterialBuffer->CopyData(mat->GetMatCBIndex(), mat->GetMaterialData());

            // Next FrameResource need to be updated too.
            mat->DecrementNumFramesDirty();
        }
    }
}

// ------------------------------------------------------------------
// Update the the main light's view matrix and projection matrix.
// ------------------------------------------------------------------
void Game::UpdateShadowTransform(const GameTimer& gt)
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
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

    XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

    // Main pass constant buffer
    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));

    mMainPassCB.EyePosW = mCamera.GetPosition3f();
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;

    // Time
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();

    // Lights
    //mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
    mMainPassCB.Lights[0].Strength = { 0.9f, 0.8f, 0.7f };
    //mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
    //mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    //mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
    //mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void Game::UpdateShadowPassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mLightView);
    XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    UINT w = mShadowMap->Width();
    UINT h = mShadowMap->Height();

    XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mShadowPassCB.EyePosW = mLightPosW;
    mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
    mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
    mShadowPassCB.NearZ = mLightNearZ;
    mShadowPassCB.FarZ = mLightFarZ;

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(1, mShadowPassCB);
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
    std::vector<std::string> texNames =
    {
        "bricksTex",
        "waterTex",
        "iceTex",
        "grassTex",
        "whiteTex",
        "crate01Tex",
        "crate02Tex",
        "checkboardTex",
        "tileTex",
        "skyCubeMap",
    };

    std::vector<std::wstring> texFilenames =
    {
        L"bricks.dds",
        L"water1.dds",
        L"ice.dds",
        L"grass.dds",
        L"white1x1.dds",
        L"WoodCrate01.dds",
        L"WoodCrate02.dds",
        L"checkboard.dds",
        L"tile.dds",
        L"Skyboxes/sunsetcube1024.dds",
    };

    mTextures = make_unique<TextureWrapper>();
    for (int i = 0; i < (int)texNames.size(); i++)
    {
        mTextures->CreateDDSTextureFromFile(md3dDevice, mCommandList, texNames[i], texFilenames[i]);
    }
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
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 99, 2, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

    // Create root CBVs.
    // Performance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsShaderResourceView(0, 1);
    slotRootParameter[2].InitAsShaderResourceView(1, 1);
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
    // Fill out the heap with actual descriptors.
    //
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("bricksTex").Get(),     D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("waterTex").Get(),      D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("crate01Tex").Get(),    D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("crate02Tex").Get(),    D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("iceTex").Get(),        D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("grassTex").Get(),      D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("whiteTex").Get(),      D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("checkboardTex").Get(), D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("tileTex").Get(),       D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);

    // Sky cube map
    mSkyTexHeapIndex = mCbvSrvUavDescriptorHeap->GetLastDescIndex();
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, mTextures->GetTextureResource("skyCubeMap").Get(), D3D12_SRV_DIMENSION_TEXTURECUBE, CUBE_MAP);

    // Shadow map
    auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    mShadowMapHeapIndex = mCbvSrvUavDescriptorHeap->GetLastDescIndex();
    mShadowMap->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));

    // Null cube
    mNullCubeSrvIndex = mCbvSrvUavDescriptorHeap->GetLastDescIndex();
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, nullptr, D3D12_SRV_DIMENSION_TEXTURECUBE, CUBE_MAP);

    // Null texture
    mNullTexSrvIndex = mCbvSrvUavDescriptorHeap->GetLastDescIndex();
    mCbvSrvUavDescriptorHeap->CreateSrvDescriptor(md3dDevice, nullptr, D3D12_SRV_DIMENSION_TEXTURE2D, DIFFUSE_MAP);

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

    const std::wstring shaderFolderPath = L"..\\..\\Engine\\Engine\\Shaders\\";

    mShaders["standardVS"] = DXUtil::CompileShader(shaderFolderPath + L"Default.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = DXUtil::CompileShader(shaderFolderPath + L"Default.hlsl", nullptr, "PS", "ps_5_1");
    
    mShaders["shadowVS"] = DXUtil::CompileShader(shaderFolderPath + L"Shadows.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["shadowOpaquePS"] = DXUtil::CompileShader(shaderFolderPath + L"Shadows.hlsl", nullptr, "PS", "ps_5_1");
    mShaders["shadowAlphaTestedPS"] = DXUtil::CompileShader(shaderFolderPath + L"Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mShaders["skyVS"] = DXUtil::CompileShader(shaderFolderPath + L"Sky.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["skyPS"] = DXUtil::CompileShader(shaderFolderPath + L"Sky.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    // PSO for shadow map pass.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;
    smapPsoDesc.RasterizerState.DepthBias = 100000;
    smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    smapPsoDesc.pRootSignature = mRootSignature.Get();
    smapPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
        mShaders["shadowVS"]->GetBufferSize()
    };
    smapPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
        mShaders["shadowOpaquePS"]->GetBufferSize()
    };

    // Shadow map pass does not have a render target.
    smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    smapPsoDesc.NumRenderTargets = 0;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));


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

    ////
    //// PSO for alpha tested objects
    ////
    //D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
    //alphaTestedPsoDesc.PS =
    //{
    //    reinterpret_cast<BYTE*>(mShaders["shadowAlphaTestedPS"]->GetBufferPointer()),
    //    mShaders["shadowAlphaTestedPS"]->GetBufferSize()
    //};
    //alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    //ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["shadowAlphaTestedPS"])));
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
            2, mInstanceCounts, mMaterials->GetSize(), mGeoBuilder->GetWaves()->VertexCount()));
    }
}

// ------------------------------------------------------------------
// Define the properties of each unique material and put them in a table.
// ------------------------------------------------------------------
void Game::BuildMaterials()
{
    // Initialize material wrapper
    mMaterials = std::make_unique<MaterialWrapper>();

    auto bricks = Material::Create("bricks");
    bricks->SetMatCBIndex(0);
    bricks->SetDiffuseSrvHeapIndex(0);
    bricks->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    bricks->SetFresnel(XMFLOAT3(0.02f, 0.02f, 0.02f));
    bricks->SetRoughness(0.1f);
    mMaterials->AddMaterial(bricks);

    auto water = Material::Create("water");
    water->SetMatCBIndex(1);
    water->SetDiffuseSrvHeapIndex(1);
    water->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f));
    water->SetFresnel(XMFLOAT3(0.2f, 0.2f, 0.2f));
    water->SetRoughness(0.2f);
    mMaterials->AddMaterial(water);

    auto crate01 = Material::Create("crate01");
    crate01->SetMatCBIndex(2);
    crate01->SetDiffuseSrvHeapIndex(2);
    crate01->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    crate01->SetFresnel(XMFLOAT3(0.1f, 0.1f, 0.1f));
    crate01->SetRoughness(0.5f);
    mMaterials->AddMaterial(crate01);

    auto crate02 = Material::Create("crate02");
    crate02->SetMatCBIndex(3);
    crate02->SetDiffuseSrvHeapIndex(3);
    crate02->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    crate02->SetFresnel(XMFLOAT3(0.1f, 0.1f, 0.1f));
    crate02->SetRoughness(0.5f);
    mMaterials->AddMaterial(crate02);

    auto ice = Material::Create("ice");
    ice->SetMatCBIndex(4);
    ice->SetDiffuseSrvHeapIndex(4);
    ice->SetDiffuseAlbedo(XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f));
    ice->SetFresnel(XMFLOAT3(0.98f, 0.97f, 0.95f));
    ice->SetRoughness(0.1f);
    mMaterials->AddMaterial(ice);

    auto grass = Material::Create("grass");
    grass->SetMatCBIndex(5);
    grass->SetDiffuseSrvHeapIndex(5);
    grass->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    grass->SetFresnel(XMFLOAT3(0.05f, 0.05f, 0.05f));
    grass->SetRoughness(0.2f);
    mMaterials->AddMaterial(grass);

    auto mirror = Material::Create("mirror");
    mirror->SetMatCBIndex(6);
    mirror->SetDiffuseSrvHeapIndex(6);
    mirror->SetDiffuseAlbedo(XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f));
    mirror->SetFresnel(XMFLOAT3(0.98f, 0.97f, 0.95f));
    mirror->SetRoughness(0.1f);
    mMaterials->AddMaterial(mirror);

    auto checkboard = Material::Create("checkboard");
    checkboard->SetMatCBIndex(7);
    checkboard->SetDiffuseSrvHeapIndex(7);
    checkboard->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    checkboard->SetFresnel(XMFLOAT3(0.1f, 0.1f, 0.1f));
    checkboard->SetRoughness(1.0f);
    mMaterials->AddMaterial(checkboard);

    auto tile = Material::Create("tile");
    tile->SetMatCBIndex(8);
    tile->SetDiffuseSrvHeapIndex(8);
    tile->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    tile->SetFresnel(XMFLOAT3(0.05f, 0.05f, 0.05f));
    tile->SetRoughness(1.0f);
    mMaterials->AddMaterial(tile);

    auto sky = Material::Create("sky");
    sky->SetMatCBIndex(9);
    sky->SetDiffuseSrvHeapIndex(9);
    sky->SetDiffuseAlbedo(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    sky->SetFresnel(XMFLOAT3(0.1f, 0.1f, 0.1f));
    sky->SetRoughness(1.0f);
    mMaterials->AddMaterial(sky);
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
    skyRitem->Instances[0].MaterialIndex = mMaterials->GetMaterial("sky")->GetMatCBIndex();

    skyRitem->instanceBufferID = instanceBufferID++;
    mInstanceCounts.push_back(instanceCount);
    totalInstanceCount += instanceCount;
    mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());


    // 2 - Cylinder render item
    auto cylinderRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&cylinderRitem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
    cylinderRitem->ObjCBIndex = 2;
    cylinderRitem->Geo = mGeoBuilder->GetMeshGeo("shapeGeo");
    cylinderRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    cylinderRitem->InstanceCount = 0;
    cylinderRitem->IndexCount = cylinderRitem->Geo->DrawArgs["cylinder"].IndexCount;
    cylinderRitem->StartIndexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
    cylinderRitem->BaseVertexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
    cylinderRitem->Bounds = cylinderRitem->Geo->DrawArgs["cylinder"].Bounds;

    // Generate instance data for box render item.
    const int n = 1;
    instanceCount = n * n * n;
    cylinderRitem->Instances.resize(instanceCount);

    float width = 25.0f;
    float height = 35.0f;
    float depth = 25.0f;

    float x = -0.5f * width;
    float y = 5.0f;
    float z = -0.5f * depth;
    float dx = width / n;
    float dy = height / n / 2;
    float dz = depth / n;
    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                int index = k * n * n + i * n + j;

                auto cylinderTransform = XMMatrixScaling(2.0f, 2.0f, 2.0f);
                cylinderTransform *= XMMatrixRotationX(index % 6 * 10.0f);
                cylinderTransform *= XMMatrixRotationZ(index % 6 * 15.0f);
                cylinderTransform *= XMMatrixTranslation(x + j * dx, y + i * dy, z + k * dz);
                XMStoreFloat4x4(&cylinderRitem->Instances[index].World, cylinderTransform);

                XMStoreFloat4x4(&cylinderRitem->Instances[index].TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.0f));
                cylinderRitem->Instances[index].MaterialIndex = index % 6 + 1;
            }
        }
    }

    cylinderRitem->instanceBufferID = instanceBufferID++;
    mInstanceCounts.push_back(instanceCount);
    totalInstanceCount += instanceCount;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(cylinderRitem.get());


    // 3 - Floor (grid)
    auto floorRitem = std::make_unique<RenderItem>();
    floorRitem->World = MathHelper::Identity4x4();
    floorRitem->ObjCBIndex = 3;
    floorRitem->Geo = mGeoBuilder->GetMeshGeo("shapeGeo");
    floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    floorRitem->IndexCount = floorRitem->Geo->DrawArgs["grid"].IndexCount;
    floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    floorRitem->Bounds = floorRitem->Geo->DrawArgs["grid"].Bounds;

    // Only one floor needed
    instanceCount = 1;
    floorRitem->Instances.resize(instanceCount);
    XMStoreFloat4x4(&floorRitem->Instances[0].World, XMMatrixScaling(2.2f, 1.0f, 2.0f));
    XMStoreFloat4x4(&floorRitem->Instances[0].TexTransform, XMMatrixScaling(7.0f, 7.0f, 7.0f));
    floorRitem->Instances[0].MaterialIndex = mMaterials->GetMaterial("tile")->GetMatCBIndex();

    floorRitem->instanceBufferID = instanceBufferID++;
    mInstanceCounts.push_back(instanceCount);
    totalInstanceCount += instanceCount;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());


    // 4 - Car Model
    auto carRitem = std::make_unique<RenderItem>();
    carRitem->World = MathHelper::Identity4x4();
    carRitem->ObjCBIndex = 4;
    carRitem->Geo = mGeoBuilder->GetMeshGeo("carModel");
    carRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    carRitem->IndexCount = carRitem->Geo->DrawArgs["carModel"].IndexCount;
    carRitem->StartIndexLocation = carRitem->Geo->DrawArgs["carModel"].StartIndexLocation;
    carRitem->BaseVertexLocation = carRitem->Geo->DrawArgs["carModel"].BaseVertexLocation;
    carRitem->Bounds = carRitem->Geo->DrawArgs["carModel"].Bounds;

    // Only one car model needed
    instanceCount = 1;
    carRitem->Instances.resize(instanceCount);
    XMStoreFloat4x4(&carRitem->Instances[0].World, XMMatrixScaling(2.5f, 2.5f, 2.5f) * XMMatrixTranslation(0.0f, 5.0f, 0.0f));
    carRitem->Instances[0].MaterialIndex = mMaterials->GetMaterial("mirror")->GetMatCBIndex();

    carRitem->instanceBufferID = instanceBufferID++;
    mInstanceCounts.push_back(instanceCount);
    totalInstanceCount += instanceCount;
    mRitemLayer[(int)RenderLayer::Opaque].push_back(carRitem.get());

    // Push all render items to list
    mAllRitems.push_back(std::move(cylinderRitem));
    mAllRitems.push_back(std::move(skyRitem));
    mAllRitems.push_back(std::move(floorRitem));
    mAllRitems.push_back(std::move(carRitem));
}

#pragma endregion


// ------------------------------------------------------------------
// Draw call for the shadow map pass.
// ------------------------------------------------------------------
void Game::DrawSceneToShadowMap()
{
    mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
    mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

    // Change to DEPTH_WRITE.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    UINT passCBByteSize = DXUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Set null render target because we are only going to draw to
    // depth buffer.  Setting a null render target will disable color writes.
    // Note the active PSO also must specify a render target count of 0.
    mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

    // Bind the pass constant buffer for the shadow map pass.
    auto passCB = mCurrFrameResource->PassCB->Resource();
    D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
    mCommandList->SetGraphicsRootConstantBufferView(0, passCBAddress);

    mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    // Change back to GENERIC_READ so we can read the texture in a shader.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

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
        mCommandList->SetGraphicsRootShaderResourceView(1, instanceBuffer->GetGPUVirtualAddress());

        cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

// ------------------------------------------------------------------
// Draw customized GUI windows using ImGui framework.
// ------------------------------------------------------------------
void Game::DrawGUI()
{
    //ImGui::ShowDemoWindow();

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
        if (mFrustumCullingEnabled)
            ImGui::Text("%i objects visible out of %i", totalVisibleInstanceCount, totalInstanceCount);
        else
            ImGui::Text("Disabled");
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
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Game::GetStaticSamplers()
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

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
        6,                                 // shaderRegister
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
        0.0f,                              // mipLODBias
        16,                                // maxAnisotropy
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp,
        shadow
    };
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