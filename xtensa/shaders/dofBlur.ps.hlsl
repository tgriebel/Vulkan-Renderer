#include "globals.h"
#include "lightUtil.h"

static const int TAP_COUNT = 4;
static const float STEP_SIZE = 1.0f;  // pixels per tap

struct dofBlur_t
{
    float4 padding;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofBlur_t )

void MaxAccumulateSample( const float3 sampleColor, const float sampleCoc,
                          inout float3 bestColor, inout float bestLuminance )
{
    const float lum = LuminanceFromRGB( sampleColor ) * abs( sampleCoc );
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
    Texture2D cocMap = localTextures[ 2 ];

    const float3 sourceColor = colorMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).rgb;
    const float pixelCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;

    float3 bestColor = sourceColor;
    float bestLuminance = LuminanceFromRGB( sourceColor ) * abs( pixelCoc );

    const float2 pixelSize = dimensions.zw;

    if ( pass == 0 )
    {
        for ( uint i = 1; i <= TAP_COUNT; ++i )
        {
            const float2 step = float2( pixelSize.x * i * STEP_SIZE, 0.0f );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv + step, 0 ).rgb, cocMap.SampleLevel( bilinearSamplerClampEdge, uv + step, 0 ).r, bestColor, bestLuminance );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv - step, 0 ).rgb, cocMap.SampleLevel( bilinearSamplerClampEdge, uv - step, 0 ).r, bestColor, bestLuminance );
        }
    }
    else
    {
        for ( uint i = 1; i <= TAP_COUNT; ++i )
        {
            const float2 step = float2( 0.0f, pixelSize.y * i * STEP_SIZE );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv + step, 0 ).rgb, cocMap.SampleLevel( bilinearSamplerClampEdge, uv + step, 0 ).r, bestColor, bestLuminance );
            MaxAccumulateSample( colorMap.SampleLevel( bilinearSamplerClampEdge, uv - step, 0 ).rgb, cocMap.SampleLevel( bilinearSamplerClampEdge, uv - step, 0 ).r, bestColor, bestLuminance );
        }
    }

    output.outColor = float4( bestColor, 1.0f );
    return output;
}
