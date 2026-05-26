#include "globals.h"

static const uint SAMPLE_COUNT = 49;
static const float PI = 3.14159265358979f;

struct dofBokeh_t
{
    float4  tileMapDimensions;
    float4  samples[ ( SAMPLE_COUNT / 2 ) + 1 ];
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofBokeh_t )

float2 UnpackSample( const uint index )
{
    const float4 packedSample = imageProcess.samples[ index / 2 ];
    
    const float2 unpackedSample = ( index % 2 ) == 0 ? packedSample.xy : packedSample.zw; 
    return unpackedSample;
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;
    
    Texture2D colorMap = localTextures[ 1 ];
    Texture2D tileMap = localTextures[ 2 ];
    Texture2D cocMap = localTextures[ 3 ];
    
    const float2 uv = input.uv0.xy;
    
    const float2 planesCoc = tileMap.SampleLevel( nearestSampler, uv, 0.0f ).rg;

    const float pixelCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).r;
    const float pixelCocRadius = abs( pixelCoc );
    const bool pixelForeground = ( pixelCoc < 0.0f );

    // Near field: Tile is approximation of scatter-as-gather
    // Far field: Using per-pixel CoC for the time being
    const float cocTileRadius = pixelForeground ? abs( planesCoc.r ) : pixelCocRadius;

    float3 colorSum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for ( uint i = 0; i < SAMPLE_COUNT; ++i )
    {
        const float2 offset = UnpackSample( i ) * dimensions.zw;
        const float2 sampleUV = input.uv0.xy + offset * cocTileRadius;

        const float3 sceneColor = colorMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rgb;
        const float sampleCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).r;

        const float weight = ( sign( sampleCoc ) == sign( pixelCoc ) )
            ? min( abs( sampleCoc ), pixelCocRadius )
            : 0.0f;

        colorSum += sceneColor * weight;
        weightSum += weight;
    }

    output.outColor = float4( colorSum.rgb / max( weightSum, 1e-4f ), 1.0f);
    return output;
}
