// Ground Truth Ambient Occlusion
// Jimenez et al., "Practical Real-Time Strategies for Accurate Indirect Occlusion", SIGGRAPH 2016
// Also: https://github.com/GameTechDev/XeGTAO/blob/master/Source/Rendering/Shaders/XeGTAO.hlsli

#include "globals.h"
#include "util.h"

struct GTAOConstants
{
    float radius; // World-space sampling radius (meters)
    uint numSlices; // Integration slices — 2 (fast) to 4 (quality)
    uint numSteps; // Steps per slice   — 4 (fast) to 8 (quality)
    uint pad;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, GTAOConstants )

// localTextures[0] = resolved depth buffer


// Step outward from startUV in stepUV increments and return the maximum
// elevation angle (radians) above the tangent plane of V.
// Samples beyond the search radius have their contribution smoothly faded to -PI/2
// so distant geometry does not produce a hard horizon cutoff.
float FindHorizonAngle( float3 P, float3 V, float2 startUV, float2 stepUV, float4x4 proj )
{
    float maxSinTheta = -1.0f;

    for ( uint i = 1; i <= imageProcess.numSteps; ++i )
    {
        const float2 sampleUV = startUV + stepUV * float( i );
        const float sampleDepth = localTextures[ 0 ].SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).r;

        if ( sampleDepth <= 0.0f )
            break;

        const float3 S = ReconstructViewPos( sampleUV, sampleDepth, proj );
        const float3 H = S - P;
        const float dist = length( H );

        // Smooth falloff: blend sinTheta toward -1 at the radius boundary
        const float weight = saturate( 1.0f - dist / imageProcess.radius );
        const float sinTheta = dot( H, V ) / max( dist, 1e-5f );

        maxSinTheta = max( maxSinTheta, lerp( -1.0f, sinTheta, weight ) );
    }

    return asin( clamp( maxSinTheta, -1.0f, 1.0f ) );
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 uv = input.uv0.xy;
    const int2 pixelPos = int2( dimensions.xy * uv );

    psOutput_t output = (psOutput_t)0;

    const float depth = localTextures[ 0 ].Load( int3( pixelPos, 0 ) ).r;

    // Sky / background — no occlusion
    if ( depth <= 0.0f )
    {
        output.outColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
        return output;
    }

    const float4x4 proj = views[ 0 ].projMat;
    const float3 P = ReconstructViewPos( uv, depth, proj );
    const float3 V = normalize( -P ); // View direction (toward camera)
    const float3 N = ReconstructNormal( localTextures[ 0 ], bilinearSamplerClampEdge, uv, dimensions, proj );

    // Convert world-space radius to UV-space extent at this pixel's depth.
    // P.z is negative in view space so viewDepth = -P.z is positive.
    // proj[0][0] = 1/tan(fovX/2), proj[1][1] = 1/tan(fovY/2).
    // A radius r at depth d spans r * proj[i][i] / d in NDC, halved for UV.
    const float viewDepth = -P.z;
    const float2 radiusUV = float2( imageProcess.radius * proj[ 0 ][ 0 ],
                                     imageProcess.radius * proj[ 1 ][ 1 ] )
                             / viewDepth * 0.5f;

    // Per-pixel noise rotates the slice directions for spatial dithering
    const float noise = InterleavedGradientNoise( float2( pixelPos ) );

    float ao = 0.0f;

    for ( uint sliceIdx = 0; sliceIdx < imageProcess.numSlices; ++sliceIdx )
    {
        // Distribute slices uniformly over [0, PI) then rotate by noise
        const float phi = PI * ( ( float( sliceIdx ) + noise ) / float( imageProcess.numSlices ) );
        const float2 omega_s = float2( cos( phi ), sin( phi ) ); // 2D screen-space direction
        const float3 omega3 = float3( omega_s, 0.0f ); // Lifted to view space (z=0)

        // Axis perpendicular to the slice plane — used to project N
        const float3 axis = normalize( cross( omega3, V ) );

        // Project N onto the slice plane (the plane spanned by omega3 and V)
        const float3 n_proj = N - dot( N, axis ) * axis;
        const float n_proj_len = length( n_proj );

        // Gamma: signed angle from V to n_proj inside the slice plane.
        // Positive when n_proj tilts toward +omega, negative toward -omega.
        const float3 n_proj_norm = n_proj / max( n_proj_len, 1e-4f );
        const float cos_gamma = clamp( dot( n_proj_norm, V ), -1.0f, 1.0f );
        const float gamma = sign( dot( n_proj, omega3 ) ) * acos( cos_gamma );

        // UV step for the horizon search
        const float2 stepUV = omega_s * radiusUV / float( imageProcess.numSteps );

        // Horizon angles on each side of P
        float h1 = FindHorizonAngle( P, V, uv, stepUV, proj );
        float h2 = FindHorizonAngle( P, V, uv, -stepUV, proj );

        // Clamp horizons to the visible hemisphere of the surface normal
        // (Jimenez 2016, eq. 9): prevents leaking from surfaces facing away
        h1 = max( h1, gamma - 0.5f * PI );
        h2 = max( h2, gamma - 0.5f * PI );

        // Analytic visibility integration (Jimenez 2016, eq. 11)
        ao += n_proj_len * 0.25f * (
            ( -cos( 2.0f * h1 - gamma ) + cos( gamma ) + 2.0f * h1 * sin( gamma ) ) +
            ( -cos( 2.0f * h2 - gamma ) + cos( gamma ) + 2.0f * h2 * sin( gamma ) )
        );
    }

    // Average over slices and invert (1 = fully lit, 0 = fully occluded)
    ao = saturate( 1.0f - ao / float( imageProcess.numSlices ) );

    output.outColor = float4( ao, ao, ao, 1.0f );
    return output;
}