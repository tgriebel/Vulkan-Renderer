// Crytek SSAO
// Based on "Finding Next Gen — CryEngine 2", Karol Mattsson, SIGGRAPH 2007.
//
// Core idea: for each pixel, cast N sample rays into the hemisphere above the
// surface.  A sample occludes the pixel when its reconstructed depth is in front
// of the pixel's depth.  AO = fraction of non-occluded samples.
//
// Additions over the original bare-minimum version:
//   - Fibonacci lattice kernel  : well-distributed samples, no repetition
//   - Normal-aligned hemisphere : eliminates samples pointing into the surface
//   - Range check               : suppresses dark halos at depth discontinuities

#include "globals.h"
#include "util.h"

struct SSAOConstants
{
    float   radius;      // World-space sampling radius (meters)
    uint    numSamples;  // Sample count — 8 (fast) to 32 (quality)
    float   bias;        // Depth bias to prevent self-occlusion on flat surfaces (meters)
    float   strength;    // AO multiplier: 1 = standard, higher = darker
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, SSAOConstants )

// localTextures[0] = resolved depth buffer


// Evenly-distributed point on the unit hemisphere using the Fibonacci lattice.
// Index i in [0, n) — adjacent indices have low correlation, giving a good
// spatial distribution for any sample count without needing a noise texture.
float3 FibonacciHemisphere( uint i, uint n )
{
    // Golden angle in radians: 2*PI * (1 - 1/phi)
    const float goldenAngle = 2.399963229f;

    const float theta = goldenAngle * float( i );
    const float t     = ( float( i ) + 0.5f ) / float( n );
    const float r     = sqrt( t );  // sqrt maps uniform area to hemisphere projection

    float sinT, cosT;
    sincos( theta, sinT, cosT );

    // xy = disk footprint,  z = hemisphere lift
    return float3( r * cosT, r * sinT, sqrt( max( 0.0f, 1.0f - t ) ) );
}


// Rotate a 2D vector by angle (radians).
float2 Rotate2D( float2 v, float angle )
{
    float s, c;
    sincos( angle, s, c );
    return float2( c * v.x - s * v.y, s * v.x + c * v.y );
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 uv       = input.uv0.xy;
    const int2   pixelPos = int2( dimensions.xy * uv );

    psOutput_t output = (psOutput_t)0;
    
    const uint SourceImageIx = 0;
    const uint ResourceImageIx0 = 1;
    const uint ResourceImageIx1 = 2;
    const uint ResourceImageIx2 = 3;

    const float depth = localTextures[ ResourceImageIx0 ].Load( int3( pixelPos, 0 ) ).r;

    // Sky / background — fully lit, no geometry to occlude
    if ( depth <= 0.0f )
    {
        output.outColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
        return output;
    }

    const float4x4 proj = views[ 0 ].projMat;
    const float3   P    = ReconstructViewPos( uv, depth, proj );
    //const float3   N    = ReconstructNormal( localTextures[ 0 ], bilinearSamplerClampEdge, uv, dimensions, proj );
    const float3 N = OctDecode( localTextures[ ResourceImageIx1 ].SampleLevel( bilinearSamplerClampEdge, uv, 0 ).ba );

    // Build a tangent frame so Fibonacci samples live in the hemisphere above N.
    float3 T, B;
    BuildTBN( N, T, B );

    // UV-space extent of the sampling radius at this pixel's depth.
    // viewDepth = -P.z because ReconstructViewPos returns z < 0 for visible geometry.
    // proj[0][0] = 1/tan(fovX/2),  proj[1][1] = 1/tan(fovY/2).
    const float  viewDepth = -P.z;
    const float2 radiusUV  = float2( imageProcess.radius * proj[ 0 ][ 0 ],
                                     imageProcess.radius * proj[ 1 ][ 1 ] )
                             / viewDepth * 0.5f;

    // Per-pixel rotation — breaks up the Fibonacci pattern so the fixed kernel
    // doesn't produce structured banding across neighboring pixels.
    const float rotAngle = InterleavedGradientNoise( float2( pixelPos ) ) * 2.0f * PI;

    float occluded = 0.0f;

    for ( uint i = 0; i < imageProcess.numSamples; ++i )
    {
        // Unit hemisphere sample in tangent space (z points along N).
        float3 s = FibonacciHemisphere( i, imageProcess.numSamples );

        // Rotate the disk component by per-pixel noise.
        s.xy = Rotate2D( s.xy, rotAngle );

        // Transform into view space: s.x along T, s.y along B, s.z along N.
        // Every sample is guaranteed to lie in the visible hemisphere of the surface.
        const float3 dir = s.x * T + s.y * B + s.z * N;

        // Project the view-space direction to a UV-space offset and sample depth.
        const float2 sampleUV    = uv + dir.xy * radiusUV;
        const float  sampleDepth = localTextures[ 0 ].SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).r;

        if ( sampleDepth <= 0.0f )
            continue;  // Sky behind the sample — skip

        const float3 S = ReconstructViewPos( sampleUV, sampleDepth, proj );

        // --- Occlusion test ---
        // S occludes P when S is in front of P (closer to camera = larger Z,
        // since view-space Z is negative for visible geometry).
        // The bias ensures a flat surface doesn't self-occlude: the sample
        // must be at least `bias` meters closer than P to count.
        const bool inFront = ( S.z >= P.z + imageProcess.bias );

        // --- Range check ---
        // When S is much further from P than the sampling radius the sample
        // shouldn't contribute — otherwise thin foreground objects cast large
        // halos over the background.
        const float rangeWeight = saturate( 1.0f - abs( P.z - S.z ) / imageProcess.radius );

        occluded += inFront ? rangeWeight : 0.0f;
    }

    // Average, apply strength, then invert so 1 = fully lit, 0 = fully occluded.
    const float ao = saturate( 1.0f - ( occluded / float( imageProcess.numSamples ) ) * imageProcess.strength );


    
    output.outColor = float4( ao, ao, ao, 1.0f );
    output.outColor.rgb = EncodeNormal( N );
    output.outColor.a = 1.0f;
    return output;
}