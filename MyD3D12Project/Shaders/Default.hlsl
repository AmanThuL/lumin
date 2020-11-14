/*=============================================================================
    Default.hlsl: Transforms and colors geometry.
=============================================================================*/

// Per Object Constant Buffer
// - Only store constants that are associated with an object
// - So far, the only constant data is the object's world matrix
// - Implicitly padded to 256 bytes
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

// Pass Constant Buffer
// - Stores constant data that is fixed over a given rendering pass
// - E.g. eye position, view/projection matrices, screen (render target)
//        dimensions, game timing info, etc.
// - Implicitly padded to 256 bytes
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

// Struct representing a single vertex worth of data
// - This should match the vertex definition in our C++ code
// - By "match", I mean the size, order and number of members
// - The name of the struct itself is unimportant, but should be descriptive
// - Each variable must have a semantic, which defines its usage
struct VertexIn
{
    // Data type
	//  |
	//  |   Name          Semantic
	//  |    |                |
	//  v    v                v
    float3 PosL			: POSITION;		// XYZ position
    float4 Color		: COLOR;		// RGBA color
};

// Struct representing the data we're sending down the pipeline
// - Should match our pixel shader's input
// - At a minimum, we need a piece of data defined tagged as SV_POSITION
// - The name of the struct itself is unimportant, but should be descriptive
// - Each variable must have a semantic, which defines its usage
struct VertexOut
{
    // Data type
	//  |
	//  |   Name          Semantic
	//  |    |                |
	//  v    v                v
    float4 PosH			: SV_POSITION;	// XYZW position (System Value Position)
    float4 Color		: COLOR;		// RGBA color
};

// --------------------------------------------------------
// The entry point (VS method) for our vertex shader
// 
// - Input is exactly one vertex worth of data (defined by a struct)
// - Output is a single struct of data to pass down the pipeline
// --------------------------------------------------------
VertexOut VS(VertexIn vin)
{
    VertexOut vout;
        
    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);   // The multiplication is negligible on modern GPUs
    
    // Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

// --------------------------------------------------------
// The entry point (PS method) for our pixel shader
// 
// - Input is the data coming down the pipeline (defined by the struct)
// - Output is a single color (float4)
// - Has a special semantic (SV_TARGET), which means 
//    "put the output of this into the current render target"
// --------------------------------------------------------
float4 PS(VertexOut pin) : SV_Target
{
    // During rasterization vertex attributes output from the VS (or GS)
    // are interpolated across the pixels of a triangle. The interpolated
    // values are then fed into the pixel shader as input.
        
    return pin.Color;
}