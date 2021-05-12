//*******************************************************************
// MathUtil.hlsl:
//
// Contains common math operations in shaders.
//*******************************************************************

#define EPS 1e-3
#define PI 3.141592653589793
#define PI2 6.283185307179586

#define NUM_SAMPLES 25
#define NUM_RINGS 10

static float2 poissonDisk[NUM_SAMPLES];

static float2 poissonDiskExp[16] =
{
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

float Rand_1to1(float x)
{
    // -1 -1
    return frac(sin(x) * 10000.0);
}

float Rand_2to1(float2 uv)
{
    // 0 - 1
    const min16float a = 12.9898, b = 78.233, c = 43758.5453;
    float dt = dot(uv.xy, float2(a, b)), sn = fmod(dt, PI);
    return frac(sin(sn) * c);
}

void PoissonDiskSamples(const in float2 randomSeed)
{
    float ANGLE_STEP = PI2 * float(NUM_RINGS) / float(NUM_SAMPLES);
    float INV_NUM_SAMPLES = 1.0 / float(NUM_SAMPLES);

    float angle = Rand_2to1(randomSeed) * PI2;
    float radius = INV_NUM_SAMPLES;
    float radiusStep = radius;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        poissonDisk[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.75);
        radius += radiusStep;
        angle += ANGLE_STEP;
    }
}