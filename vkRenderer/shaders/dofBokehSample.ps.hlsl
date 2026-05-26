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
    Texture2D cocMap = localTextures[ 3 ];
    
    const float2 uv = input.uv0.xy;
    
    float signedTileCoc = tileMap.SampleLevel( nearestSampler, uv, 0.0f ).g;

    float pixelCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).r;

    // Expand kernel bound across neighbouring tiles to cover boundary misalignment.
    // Take the signed max CoC — whichever neighbour has the largest magnitude "wins".
    [unroll]
    for ( uint i = 0; i < 8; ++i )
    {
        const float2 offsetUV = uv + neighborhoodKernelOffsets[ i ] * imageProcess.tileMapDimensions.zw;
        const float neighborCoc = tileMap.SampleLevel( nearestSampler, offsetUV, 0.0f ).g;
    //    signedTileCoc = ( abs( neighborCoc ) > abs( signedTileCoc ) ) ? neighborCoc : signedTileCoc;
    }

    const bool foreground = ( signedTileCoc < 0.0f );
    const float cocTileRadius = abs( signedTileCoc );

    float3 colorSum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for ( uint i = 0; i < SAMPLE_COUNT; ++i )
    {
        const float2 offset = UnpackSample( i ) * dimensions.zw;
        const float2 sampleUV = input.uv0.xy + offset * cocTileRadius;

        const float3 sceneColor = colorMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rgb;
        const float sampleCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).r;

        float weight = ( sign( sampleCoc ) == sign( pixelCoc ) )
             ? abs( sampleCoc )
             : min( abs( sampleCoc ), abs( pixelCoc ) );
        
        colorSum += sceneColor * weight;
        weightSum += weight;
    }

    output.outColor = float4( colorSum.rgb / max( weightSum, 1e-4f ), 1.0f);
    return output;
}
