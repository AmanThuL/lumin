
// Constant Buffer
// - Allows us to define a buffer of individual variables 
//    which will (eventually) hold data from our C++ code
// - All non-pipeline variables that get their values from 
//    our C++ code must be defined inside a Constant Buffer
// - The name of the cbuffer itself is unimportant
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
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
    
    // Transform to homogeneous clip space.
    vout.PosH = mul(float4(vin.Pos, 1.0f), gWorldViewProj);
    
    // Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}