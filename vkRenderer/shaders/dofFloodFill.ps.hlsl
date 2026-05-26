#include "globals.h"

struct dofBokeh_t
{
    float4 tileMapDimensions;
    float4 samples[ ( SAMPLE_COUNT / 2 ) + 1 ];
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofBokeh_t )

static const int TAP_COUNT = 8;
static const float STEP_SIZE = 2.0f;

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;
    
    Texture2D dofBokehMap = localTextures[ 1 ];
    Texture2D cocMap = localTextures[ 2 ];
    
    const float2 uv = input.uv0.xy;
    
    const float pixelCoc = abs( cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r );

    float3 colorAccum = 0.0f;
    float weightAccum = 0.0f;

    for ( int i = -TAP_COUNT; i <= TAP_COUNT; ++i )
    {
        const float dist = abs( i ) * STEP_SIZE;
        const float2 sampleUV = uv + float2( dist * sign( i ) * dimensions.z, 0.0f );

        const float3 color = dofBokehMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).rgb;
        const float sampleCoc = abs( cocMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).r );

        // Neighbour contributes if its disc reaches this pixel
        const float weight = saturate( sampleCoc - dist + 0.5f );

        colorAccum += color * weight;
        weightAccum += weight;
    }

    output.outColor = float4( colorAccum / max( weightAccum, 1e-4f ), 1.0f );
    return output;
}