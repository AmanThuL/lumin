
// Struct representing the data we expect to receive from earlier pipeline stages
// - Should match the output of our corresponding vertex shader
// - The name of the struct itself is unimportant
// - The variable names don't have to match other shaders (just the semantics)
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