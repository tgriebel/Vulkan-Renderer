#include "globals.h"

static const uint SAMPLE_COUNT = 49;
static const float PI = 3.14159265358979f;

struct dofBokeh_t
{
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
    
    const float tileCocRadius = localTextures[ 2 ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy, 0.0f ).b;

    float3 color = 0.0f;

    for ( uint i = 0; i < SAMPLE_COUNT; ++i )
    {     
        const bool foregrond = ( tileCocRadius >= 0 ) ? true : false;
        
        const float2 offset = UnpackSample( i ) * dimensions.zw;
        const float2 sampleUV = input.uv0.xy + offset * abs( tileCocRadius );

        color += localTextures[ 1 ].SampleLevel( bilinearSamplerClampEdge, sampleUV, 0.0f ).rgb;
    }

    output.outColor = float4( color / float( SAMPLE_COUNT ), 1.0f );
    return output;
}
