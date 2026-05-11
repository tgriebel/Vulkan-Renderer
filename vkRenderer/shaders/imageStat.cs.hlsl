#include "globals.h"
#include "util.h"

// Rough reference: https://bruop.github.io/exposure/

#define HistogramBins 16

struct ImageStatParms
{
    float4 dimensions;
    float2 luminanceRange;
    uint   imageId;
    uint   baseOffset;
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
CONSTANT_LAYOUT( 0, 2, ImageStatParms, imageStatParms )
WRITE_BUFFER_LAYOUT( 0, 3, uint, imageStatBuffer )

groupshared uint groupHistogramBins[ HistogramBins ];
groupshared int groupMinRSample;
groupshared int groupMinGSample;
groupshared int groupMinBSample;

groupshared int groupMaxRSample;
groupshared int groupMaxGSample;
groupshared int groupMaxBSample;

uint ColorToHistogramBin( float3 colorSample, float minLogLum, float inverseLogLumRange )
{
    float luminance = dot( colorSample, float3( 0.2126f, 0.7152f, 0.0722f ) );

    if ( luminance < 1e-6f ) {
        return 0;
    }

    float logLum = clamp( ( log2( luminance ) - minLogLum ) * inverseLogLumRange, 0.0f, 1.0f );

    // Map [0, 1] to [1, 255]
    return uint( logLum * 254.0f + 1.0 );
}

int QuantizeFloat( float s )
{
    // 16bit is good for HDR, 8bit is too low, 32bit requires more care regarding precision
    return clamp( s, -1.0f, 1.0f ) * 65535;
}


[numthreads( 16, 16, 1 )]
void CSMain( uint3 threadId : SV_DispatchThreadID, uint groupId : SV_GroupIndex )
{
    if ( groupId < HistogramBins )
    {
        groupHistogramBins[ groupId ] = 0;
    }
    groupMinRSample = 65535;
    groupMinGSample = 65535;
    groupMinBSample = 65535;
    
    groupMaxRSample = -65535;
    groupMaxGSample = -65535;
    groupMaxBSample = -65535;
    
    GroupMemoryBarrierWithGroupSync();

    const uint width = uint( imageStatParms.dimensions.x );
    const uint height = uint( imageStatParms.dimensions.y );
    const int lod = int( imageStatParms.dimensions.z );
    
    const int2 pixelLocation = threadId.xy;

    if ( ( pixelLocation.x < width ) && ( pixelLocation.y < height ) )
    {
        const float4 pixel = localTextures[ imageStatParms.imageId ].Load( int3( pixelLocation.xy, lod ) );
        const uint bin = ColorToHistogramBin( pixel.rgb, imageStatParms.luminanceRange.x, imageStatParms.luminanceRange.y );

        InterlockedMin( groupMinRSample, QuantizeFloat( pixel.r ) );
        InterlockedMin( groupMinGSample, QuantizeFloat( pixel.r ) );
        InterlockedMin( groupMinBSample, QuantizeFloat( pixel.r ) );
          
        InterlockedMax( groupMaxRSample, QuantizeFloat( pixel.r ) );
        InterlockedMax( groupMaxGSample, QuantizeFloat( pixel.r ) );
        InterlockedMax( groupMaxBSample, QuantizeFloat( pixel.r ) );
        
        InterlockedAdd( groupHistogramBins[ bin ], 1 );
    }
    GroupMemoryBarrierWithGroupSync();
    
    //if ( ( pixelLocation.x == imageStatParms.sampleX ) && ( pixelLocation.y == imageStatParms.sampleY ) )
    //{
    // readback sample value at location
    //}

    // Flush per-group bins to the global buffer.
    if ( groupId < HistogramBins ) {
    //    InterlockedAdd( imageStatBuffer[ imageStatParms.baseOffset + groupId ], groupHistogramBins[ groupId ] );
    }
    if ( ( pixelLocation.x < width ) && ( pixelLocation.y < height ) )
    {
        imageStatBuffer[ pixelLocation.x + pixelLocation.y * width ] = pixelLocation.x;
    }
    //InterlockedMin( imageStatBuffer[ imageStatParms.baseOffset + groupId + 0 ], groupMinRSample );
    //InterlockedMin( imageStatBuffer[ imageStatParms.baseOffset + groupId + 1 ], groupMinGSample );
    //InterlockedMin( imageStatBuffer[ imageStatParms.baseOffset + groupId + 2 ], groupMinBSample );
    //InterlockedMax( imageStatBuffer[ imageStatParms.baseOffset + groupId + 3 ], groupMaxRSample );
    //InterlockedMax( imageStatBuffer[ imageStatParms.baseOffset + groupId + 4 ], groupMaxGSample );
    //InterlockedMax( imageStatBuffer[ imageStatParms.baseOffset + groupId + 5 ], groupMaxBSample );
}
