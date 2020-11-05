
// Constant Buffer
// - Allows us to define a buffer of individual variables 
//    which will (eventually) hold data from our C++ code
// - All non-pipeline variables that get their values from 
//    our C++ code must be defined inside a Constant Buffer
// - The name of the cbuffer itself is unimportant
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4 gPulseColor;
    float gTime;
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
    float3 Pos			: POSITION;		// XYZ position
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
    
    //vin.Pos.xy += 0.5f * sin(vin.Pos.x) * sin(3.0f * gTime);
    //vin.Pos.z *= 0.6f * 0.4f * sin(2.0f * gTime);
    
    // Transform to homogeneous clip space.
    vout.PosH = mul(float4(vin.Pos, 1.0f), gWorldViewProj);
    
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
    
    const float pi = 3.14159;
    
    // Oscillate a value in [0,1] over time using a sine function.
    float s = 0.5f * sin(2 * gTime - 0.25f * pi) + 0.5f;
    
    // Linearly interpolate between pin.Color and gPulseColor based on
    // parameter s.
    float c = lerp(pin.Color, gPulseColor, s);
    
    return pin.Color;
}