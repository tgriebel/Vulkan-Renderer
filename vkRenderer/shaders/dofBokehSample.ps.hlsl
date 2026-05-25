#include "globals.h"

static const uint SAMPLE_COUNT = 49;
static const float PI = 3.14159265358979f;

struct dofBokeh_t
{
    float4  tileMapDimensions;
    float3  bias;
    float   colorScale;
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
    
    const float2 neighborhoodKernelOffsets[ 8 ] =
    {
        float2( -1.0f, -1.0f ),
        float2( 0.0f, -1.0f ),
        float2( 1.0f, -1.0f ),
        float2( -1.0f, 0.0f ),
        float2( 1.0f, 0.0f ),
        float2( -1.0f, 1.0f ),
        float2( 0.0f, 1.0f ),
        float2( 1.0f, 1.0f ),
    };
    
    Texture2D colorMap = localTextures[ 1 ];
    Texture2D tileMap = localTextures[ 2 ];
    
    const float2 uv = input.uv0.xy;
    
    float tileCocRadius = tileMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).g;
    
    [unroll]
    for ( uint i = 0; i < 8; ++i )
    {
     //   const float2 offsetUV = uv + neighborhoodKernelOffsets[ i ] * imageProcess.tileMapDimensions.zw;
    //    tileCocRadius = min( tileCocRadius, tileMap.SampleLevel( bilinearSamplerClampEdge, offsetUV, 0.0f ).b );
    }
    
    float3 color = 0.0f;
          
	[unroll]
    for ( uint i = 0; i < SAMPLE_COUNT; ++i )
    {     
        const bool foregrond = ( tileCocRadius >= 0 ) ? true : false;
        
        const float cocTileRadius = abs( tileCocRadius );
        
        const float2 offset = UnpackSample( i ) * dimensions.zw;
        const float2 sampleUV = input.uv0.xy + offset * cocTileRadius;

        const float3 sample = colorMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rgb;
        
        color += sample;
    }

    output.outColor = float4( color / float( SAMPLE_COUNT ), 1.0f );
    return output;
}
