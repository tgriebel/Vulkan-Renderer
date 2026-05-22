#include "globals.h"

struct dofBokeh_t
{
    float   cocRadius;
    float   apertureAngle;
    int     ngonBlades;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofBokeh_t )

static const float PI = 3.14159265358979f;

static const uint SAMPLE_COUNT = 16;

// Poisson disk samples in the unit disk.
// Replaced by a dynamic blue noise / Halton sequence later.
static const float2 g_poissonDisk[ SAMPLE_COUNT ] =
{
    float2( -0.6134,  0.6175 ),
    float2(  0.1700, -0.0403 ),
    float2( -0.2994,  0.7919 ),
    float2(  0.6457,  0.4932 ),
    float2( -0.6518,  0.7179 ),
    float2(  0.4210,  0.0271 ),
    float2( -0.8172, -0.2711 ),
    float2( -0.7054, -0.6682 ),
    float2(  0.9771, -0.1086 ),
    float2(  0.0633,  0.1424 ),
    float2(  0.2035,  0.2146 ),
    float2( -0.1925, -0.4039 ),
    float2( -0.3238, -0.6268 ),
    float2( -0.2139,  0.8902 ),
    float2(  0.0075, -0.8853 ),
    float2(  0.5122, -0.2494 ),
};


// Radius of a regular N-gon boundary (circumradius = 1) at angle theta.
// At a vertex:    r = 1
// At edge center: r = cos(pi/N)  (inradius)
// Graphics Gems from Cryengine 3 (Siggraph 2013)
float NGonBoundaryRadius( float theta, int N )
{
    const float piOverN = PI / float( N );
    const float twoPiOverN = 2.0f * piOverN;

    float t = theta - twoPiOverN * floor( theta / twoPiOverN ) - piOverN;

    return cos( piOverN ) / cos( t );
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const float cocRadius = imageProcess.cocRadius;
    const float apertureAngle = imageProcess.apertureAngle;
    const int blades = int( imageProcess.ngonBlades );

    float3 colorSum = 0.0f;

    for ( uint i = 0; i < SAMPLE_COUNT; ++i )
    {
        const float2 s = g_poissonDisk[ i ];

        // Compute polar coords and apply aperture rotation
        float r = length( s );
        float theta = atan2( s.y, s.x ) + apertureAngle;

        // Warp unit disk sample onto N-gon boundary
        r *= NGonBoundaryRadius( theta, blades );

        const float2 offset   = r * float2( cos( theta ), sin( theta ) );
        const float2 sampleUV = input.uv0.xy + offset * cocRadius * dimensions.zw;

        colorSum += localTextures[ 0 ].SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rgb;
    }

    output.outColor = float4( colorSum / float( SAMPLE_COUNT ), 1.0f );

    return output;
}
