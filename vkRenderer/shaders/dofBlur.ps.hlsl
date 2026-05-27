#include "globals.h"

static const uint SAMPLE_COUNT = 49;

struct dofBokeh_t
{
    float4 tileMapDimensions;
    float4 samples[ ( SAMPLE_COUNT / 2 ) + 1 ];
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofBokeh_t )

float2 UnpackSample( const uint index )
{
    const float4 packedSample = imageProcess.samples[ index / 2 ];
    
    const float2 unpackedSample = ( index % 2 ) == 0 ? packedSample.xy : packedSample.zw;
    return unpackedSample;
}

static const int TAP_COUNT = 8;
static const float STEP_SIZE = 2.0f;

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;
    
    const float2 uv = input.uv0.xy;
    
    Texture2D colorMap = localTextures[ 0 ];
    Texture2D depthMap = localTextures[ 1 ];
    Texture2D cocMap = localTextures[ 2 ];
    Texture2D tileMap = localTextures[ 3 ];
    
    float3 sourceColor = colorMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).rgb;
    float sourceDepth = depthMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;
    float sourceCocPixel = cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;
    const float2 planesCoc = tileMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).rg;

    const float pixelCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).r;
    const float pixelCocRadius = abs( pixelCoc );
    const bool pixelForeground = ( pixelCoc < 0.0f );

    const float cocTileRadius = pixelForeground ? abs( planesCoc.r ) : pixelCocRadius;
    
    float3 bestColor = sourceColor;
    
    float3 accumulator = float3( 0.0f, 0.0f, 0.0f );
    float sampleCount = 0;

    for ( uint i = 0; i < 1; ++i )
    {
        const float2 offset = UnpackSample( i ) * dimensions.zw;
        
        float2 sampleUV = uv;
        float3 sampleColor;

        if ( pass == 0 )
        {
            sampleUV.x = offset.x * cocTileRadius;
            sampleColor = colorMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).rgb;
        }
        else
        {
            Texture2D previousColorMap = localTextures[ previousImageId ];
            
            sampleUV.y = offset.y * cocTileRadius;
            sampleColor = previousColorMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).rgb;
        }
        
        const float sampleDepth = depthMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).r;
        const float sampleCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).r;
        
        const bool sampleIsCloserToCamera = ( sampleDepth > sourceDepth );
        const bool sampleIsMoreFocused = ( sampleCoc < pixelCoc );

        //if ( sampleIsCloserToCamera ) {
        //    continue;
        //}
        
        //if ( sampleIsMoreFocused ) {
        //    continue;
        //}
        
        accumulator += sampleColor;
        sampleCount += 1.0f;
    }
    
    if ( sampleCount > 0 ) {
        output.outColor = float4( accumulator / sampleCount, 1.0f );
    } else {
        output.outColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );
    }
    return output;
}