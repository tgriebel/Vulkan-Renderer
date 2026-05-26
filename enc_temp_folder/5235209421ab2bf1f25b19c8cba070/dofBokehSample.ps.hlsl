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
    
    const float2 planesCoc = tileMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).rg;

    const float pixelCoc = cocMap.SampleLevel( bilinearSamplerClampEdge, uv, 0.0f ).r;

    const bool nearPlane = ( planesCoc.r < 0.0f );
    
    const float cocTileRadius = abs( planesCoc.r );
    
    float3 colorSum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for ( uint i = 0; i < SAMPLE_COUNT; ++i )
    {
        const float2 offset = UnpackSample( i ) * dimensions.zw;
        const float2 sampleUV = input.uv0.xy + offset * cocTileRadius;

        const float3 sceneColor = colorMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rgb;
        const float2 sampleCocPlanes = cocMap.SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rg;
        
        const float sampleCoc = sampleCocPlanes.r;

        float weight = ( sign( sampleCoc ) == sign( pixelCoc ) )
             ? min( abs( sampleCoc ), abs( pixelCoc ) )
             : 0.0f;
               
        // Weight is the smaller of the two CoC radii — the destination pixel's blur
        // caps how much it receives, and cross-boundary samples are naturally damped.
        //const float weight = ( pixelForeground == sampleForeground )
        //    ? min( abs( sampleCoc ), pixelCocRadius )
        //    : min( abs( sampleCoc ), pixelCocRadius ) * 0.5f;
        
        colorSum += sceneColor * weight;
        weightSum += weight;
    }

    output.outColor = float4( colorSum.rgb / max( weightSum, 1e-4f ), 1.0f);
    return output;
}
