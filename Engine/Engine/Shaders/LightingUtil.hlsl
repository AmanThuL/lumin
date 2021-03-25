//*******************************************************************
// Copyright Frank Luna (C) 2015 All Rights Reserved.
//
// LightingUtil.hlsl:
//
// Contains API for shader lighting.
//*******************************************************************

// Struct supporting directional, point, and spot lights.
// - The order of data members is NOT arbitrary!
// - Structure padding occurs so that elements are packed into 4D vectors, with
//   the restriction that a single element cannot be split across two 4D 
//   vectors.
//      - vector 1: (Strength.x, Strength.y, Strength.z, FalloffStart)
//      - vector 2: (Direction.x, Direction.y, Direction.z, FalloffEnd)
//      - vector 3: (Position.x, Position.y, Position.z, SpotPower)

#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart; // point/spot light only
    float3 Direction; // directional/spot light only
    float FalloffEnd; // point/spot light only
    float3 Position; // point light only
    float SpotPower; // spot light only
};

// Struct with material data
struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    
    // Shininess is inverse of roughness: Shininess = 1 - roughness.
    float Shininess;
};

// ------------------------------------------------------------------
// Implements a linear attenuation factor, which applies to point lights 
// and spots lights.
// ------------------------------------------------------------------
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    // Linear falloff.
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// ------------------------------------------------------------------
// The Schlick approximation to the Fresnel equations; it approximates 
// the percentage of light reflected off a surface with normal n based 
// on the angle between the light vector L and surface normal n due to 
// the Fresnel effect.
// Reference: pg. 233 "Real-Time Rendering 3rd Ed"
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
// ------------------------------------------------------------------
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

// ------------------------------------------------------------------
// Computes the amount of light reflected into the eye; it is the sum 
// of diffuse reflectance and specular reflectance.
// ------------------------------------------------------------------
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    // Derive m from the shininess, which is derived from the roughness.
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);
    
    // (m + 8) / 8 * (n dot h)^m
    // Why (m + 8) / 8?
    // - Normalization factor to model energy conservation in the specular 
    //   reflection.
    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;
    
    // Our spec formula goes outside [0,1] range. However, our render target
    // expects color values to be in the low-dynamic-range (LDR) of [0,1].
    // Therefore, to get softer specular highlights without a clamp, we need to
    // scale down the specular albedo.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

// ------------------------------------------------------------------
// Given the eye position E and given a point p on a surface visible
// to the eye with surface normal n, and material properties, the
// function outputs the amount of light, from a directional light
// source, that reflects into the to-eye direction v = normalize(E-p).
// ------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 lightVec = -L.Direction;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// ------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
// ------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    // The vector from the surface to the light.
    float3 lightVec = L.Position - pos;
    
    // The distance from surface to light.
    float d = length(lightVec);
    
    // Range test.
    if (d > L.FalloffEnd)
        return 0.0f;
    
    // Normalize the light vector.
    lightVec /= d;
    
    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;
    
    // Attenuate light by distance.
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// ------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
// ------------------------------------------------------------------
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    // The vector from the surface to the light.
    float3 lightVec = L.Position - pos;
    
    // The distance from surface to light.
    float d = length(lightVec);
    
    // Range test.
    if (d > L.FalloffEnd)
        return 0.0f;
    
    // Normallize the light vector.
    lightVec /= d;
    
    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;
    
    // Attenuate light by distance.
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    
    // Scale by spotlight
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;
    
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// ------------------------------------------------------------------
// Iterates over each light source and sum its contribution to the
// point/pixel we are evaluating the lighting of. Uses the convention
// that directional lights must come first in the light array, point
// lights come second, and spot lights come last.
// ------------------------------------------------------------------
float4 ComputeLighting(Light gLights[MaxLights], Material mat,
                       float3 pos, float3 normal, float3 toEye,
                       float3 shadowFactor)
{
    float3 result = 0.0f;
    
    int i = 0;
    
    // The number of lights for each type is controlled with #defines. The idea
    // is for the shader to only do the lighting equation for the number of
    // lights that are actually needed.
    
    // The shadow factor will be multiplied against the direct lighting terms.
#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif 

    return float4(result, 0.0f);
}
