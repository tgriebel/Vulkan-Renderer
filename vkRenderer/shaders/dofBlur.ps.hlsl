#include "globals.h"
#include "lightUtil.h"

static const int TAP_COUNT = 3;
static const float STEP_SIZE = 1.0f;  // pixels per tap

struct dofBlur_t
{
    float4 padding;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofBlur_t )

void MaxAccumulateSample( const float3 sampleColor, inout float3 bestColor, inout float bestLuminance )
{
    const float lum = LuminanceFromRGB( sampleColor );
    if ( lum > bestLuminance )
    {
        bestLuminance = lum;
        bestColor = sampleColor;
    }
}

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const float2 uv = input.uv0.xy;
    const uint texId = ( pass == 0 ) ? 0 : previousImageId;

    Texture2D colorMap = localTextures[ texId ];

    const float3 sourceColor = colorMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).rgb;

    float3 bestColor = sourceColor;
    float bestLuminance = LuminanceFromRGB( sourceColor );

    if ( pass == 0 )
    {
        for ( uint i = 1; i <= TAP_COUNT; ++i )
        {
            const float2 step = float2( 0.5f * dimensions.z * i * STEP_SIZE, 0.0f );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv + step, 0 ).rgb, bestColor, bestLuminance );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv - step, 0 ).rgb, bestColor, bestLuminance );
        }
    }
    else
    {
        for ( uint i = 1; i <= TAP_COUNT; ++i )
        {
            const float2 step = float2( 0.0f, 0.5f * dimensions.w * i * STEP_SIZE );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv + step, 0 ).rgb, bestColor, bestLuminance );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv - step, 0 ).rgb, bestColor, bestLuminance );
        }
    }

    output.outColor = float4( bestColor, 1.0f );
    return output;
}
