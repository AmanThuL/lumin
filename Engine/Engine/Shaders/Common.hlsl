//*******************************************************************
// Copyright Frank Luna (C) 2015 All Rights Reserved.
//
// Common.hlsl:
//
// Common shader for root signatures that are shared by multiple shaders.
//*******************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"
#include "MathUtil.hlsl"

// Shadow map related variables
//#define USE_PCF
#define USE_PCSS

#define SHADOW_DEPTH_BIAS 0.004
#define PCF_NUM_SAMPLES NUM_SAMPLES

// PCSS related
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define NEAR_PLANE 10.0
#define LIGHT_WORLD_SIZE 10.0
#define LIGHT_FRUSTUM_WIDTH 1.0 

// Assuming LIGHT_FRUSTUM_WIDTH == LIGHT_FRUSTUM_HEIGHT
#define LIGHT_SIZE_UV (LIGHT_WORLD_SIZE / LIGHT_FRUSTUM_WIDTH)


struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint InstPad0;
    uint InstPad1;
    uint InstPad2;
};

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint MatPad0;
    uint MatPad1;
    uint MatPad2;
};

TextureCube gCubeMap : register(t0);
Texture2D gShadowMap : register(t1);

// An array of textures, which is only supported in shader model 5.1+. Unlike
// Texture2DArray, the textures in this array can be different sizes and
// formats, making it more flexible than texture arrays.
Texture2D gTextureMaps[99] : register(t2);

// Put in space1, so the texture array does not overlap with these resources.
// The texture array will occupy registers t0, t1, ..., t6 in space0.
StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);
StructuredBuffer<MaterialData> gMaterialData : register(t1, space1);


// Sampler objects definition     
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

// Constant data that varies per material.
cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Allow application to change fog parameters once per frame.
    // For example, we may only use fog for certain times of day.
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;
    
    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

// ================================== Shadows =================================

// Getting average blocker depth in a certain region
// Reference: http://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf
float FindBlocker(float2 uv, float zReceiver)
{
    // Uses similar triangles to compute the search area of the shadow map
    float searchWidth = LIGHT_SIZE_UV * (zReceiver - NEAR_PLANE) / zReceiver;

    int numBlockers = 0;
    float blockerSum = 0.0;

    PoissonDiskSamples(float2(Rand_1to1(uv.x), Rand_1to1(uv.y)));

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    float dx = 1.0 / (float) width;
    
    for (int i = 0; i < BLOCKER_SEARCH_NUM_SAMPLES; ++i)
    {
        float2 offset = poissonDisk[i] * searchWidth * dx;
        float shadowMapDepth = gShadowMap.Sample(gsamLinearWrap, uv + offset, 0).r;
        if (shadowMapDepth < zReceiver - SHADOW_DEPTH_BIAS)
        {
            blockerSum += shadowMapDepth;
            numBlockers++;
        }
    }

    if (numBlockers < 1)
        return 0.0;
    return blockerSum / float(numBlockers);
}

//-----------------------------------------------------------------------------
// PCF for shadow mapping.
//-----------------------------------------------------------------------------
float PCF(float4 coords, float filterRadiusUV)
{
    float currentDepth = coords.z;
    
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    
    float dx = 1.0f / (float) width; // Texel size
    float percentLit = 0.0f;
    
    PoissonDiskSamples(float2(Rand_1to1(coords.x), Rand_1to1(coords.y)));

    [unroll]
    for (int i = 0; i < PCF_NUM_SAMPLES; ++i)
    {
        float2 offset = poissonDisk[i] * 5 * dx * filterRadiusUV;
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            coords.xy + offset, currentDepth - SHADOW_DEPTH_BIAS).r;
    }
    
    return percentLit / float(PCF_NUM_SAMPLES);
}

//-----------------------------------------------------------------------------
// PCSS for shadow mapping.
//-----------------------------------------------------------------------------
float PCSS(float4 coords)
{
    float2 uv = coords.xy;
    float zReceiver = coords.z;

    // STEP 1: avgblocker depth
    float avgBlockerDepth = FindBlocker(uv, zReceiver);

    // Check if (numBlockers == 0) to save filtering
    if (avgBlockerDepth < EPS)
        return 1.0;

    // STEP 2: penumbra size
    float penumbraSize = (zReceiver - avgBlockerDepth) / avgBlockerDepth * LIGHT_SIZE_UV;

    // STEP 3: filtering
    return PCF(coords, penumbraSize);
}

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;
    
    //const float2 offsets[9] =
    //{
    //    float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
    //    float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
    //    float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    //};
    
    #ifdef USE_PCF
        return PCF(shadowPosH, 5.0);
    #endif
    #ifdef USE_PCSS
        return PCSS(shadowPosH);
    #endif
}