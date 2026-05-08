// Edge-aware separable Gaussian

#include "globals.h"
#include "util.h"

struct BilateralProcess
{
    float kDepth;
    float pad0;
    float pad1;
    float pad2;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, BilateralProcess )

static const uint  weightCount = 5;
static const float weights[ 5 ] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

float DepthWeight( float dCenter, float dSample, float k )
{
    // ReverseZ Depth (1 is near, 0 far)
    const float diff = ( dSample - dCenter ) / max( dCenter, 1e-5f );
    return exp( -( diff * diff ) * k * k );
}

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const bool horizontal = ( pass == 0 );
    const uint texId = horizontal ? 0 : previousImageId;
    const uint depthId = 1;

    const float2 uv = input.uv0.xy;
    const float2 dir = horizontal ? float2( dimensions.z, 0.0f ) : float2( 0.0f, dimensions.w );
    const float  lod = 0.0f;

    const float depthCenter = localTextures[ depthId ].SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;

    float3 sum = localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, uv, lod ).rgb * weights[ 0 ];
    float  weight = weights[ 0 ];

    for ( uint i = 1; i < weightCount; ++i )
    {
        const float2 offset = dir * float( i );

        const float2 uvPos = uv + offset;
        const float2 uvNeg = uv - offset;

        const float dPos = localTextures[ depthId ].SampleLevel( bilinearSamplerClampEdge, uvPos, 0 ).r;
        const float dNeg = localTextures[ depthId ].SampleLevel( bilinearSamplerClampEdge, uvNeg, 0 ).r;

        // Skip taps that landed on sky (far is 0).
        const float skyPos = ( dPos > 0.0f ) ? 1.0f : 0.0f;
        const float skyNeg = ( dNeg > 0.0f ) ? 1.0f : 0.0f;

        const float wPos = weights[ i ] * DepthWeight( depthCenter, dPos, imageProcess.kDepth ) * skyPos;
        const float wNeg = weights[ i ] * DepthWeight( depthCenter, dNeg, imageProcess.kDepth ) * skyNeg;

        sum += localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, uvPos, lod ).rgb * wPos;
        sum += localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, uvNeg, lod ).rgb * wNeg;
        weight += wPos + wNeg;
    }

    output.outColor = float4( sum / max( weight, 1e-5f ), 1.0f );
    output.outColor.a = 1.0f;
    return output;
}
