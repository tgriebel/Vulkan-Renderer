#include "globals.h"
#include "util.h"

// Rough reference: https://bruop.github.io/exposure/

#define HistogramBins 16

struct ImageStatParms_t
{
    float4  dimensions;
    float2  luminanceRange;
    uint2   pickLocation;
    uint    imageId;
    uint    baseOffset;
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
CONSTANT_LAYOUT( 0, 2, ImageStatParms_t, imageStatParms )
WRITE_BUFFER_LAYOUT( 0, 3, uint, imageStatBuffer )

groupshared uint groupHistogramBins[ HistogramBins ];
groupshared uint groupMinSample[ 4 ];
groupshared uint groupMaxSample[ 4 ];

// Maps float to a uint using by partitioning using 2's compliment range
uint QuantizeFloat( float f )
{
    uint u = asuint( f );
    uint mask = ( u >> 31 ) ? 0xFFFFFFFF : 0x80000000;
    return u ^ mask;
}


uint ColorToHistogramBin( float3 colorSample, float2 luminanceRange )
{
    float luminance = dot( colorSample, float3( 0.2126f, 0.7152f, 0.0722f ) );

    if ( luminance < 1e-6f ) {
        return 0;
    }

    float logLum = clamp( ( log2( luminance ) - luminanceRange.x ) / ( luminanceRange.y - luminanceRange.x ), 0.0f, 1.0f );

    // Map [0, 1] to [1, 255]
    return uint( logLum * 254.0f + 1.0 );
}


[numthreads( 16, 16, 1 )]
void CSMain( uint3 threadId : SV_DispatchThreadID, uint groupId : SV_GroupIndex )
{
    if ( groupId < HistogramBins )
    {
        groupHistogramBins[ groupId ] = 0;
    }

    if ( groupId < 4 )
    {
        groupMinSample[ groupId ] = 0xFF7FFFFFu; // QuantizeFloat( +FLT_MAX )
        groupMaxSample[ groupId ] = 0x00800000u; // QuantizeFloat( -FLT_MAX )
    }

    GroupMemoryBarrierWithGroupSync();

    const uint width = uint( imageStatParms.dimensions.x );
    const uint height = uint( imageStatParms.dimensions.y );
    const int lod = int( imageStatParms.dimensions.z );

    const int2 pixelLocation = threadId.xy;

    if ( ( pixelLocation.x < width ) && ( pixelLocation.y < height ) )
    {
        const float4 pixel = localTextures[ 0 ].Load( int3( pixelLocation.xy, lod ) );
        const uint bin = ColorToHistogramBin( pixel.rgb, imageStatParms.luminanceRange );

        const uint4 uPixel = uint4( QuantizeFloat( pixel.r ),
                                    QuantizeFloat( pixel.g ),
                                    QuantizeFloat( pixel.b ),
                                    QuantizeFloat( pixel.a ) );

        InterlockedMin( groupMinSample[ 0 ], uPixel.r );
        InterlockedMin( groupMinSample[ 1 ], uPixel.g );
        InterlockedMin( groupMinSample[ 2 ], uPixel.b );
        InterlockedMin( groupMinSample[ 3 ], uPixel.a );

        InterlockedMax( groupMaxSample[ 0 ], uPixel.r );
        InterlockedMax( groupMaxSample[ 1 ], uPixel.g );
        InterlockedMax( groupMaxSample[ 2 ], uPixel.b );
        InterlockedMax( groupMaxSample[ 3 ], uPixel.a );

        InterlockedAdd( groupHistogramBins[ bin ], 1 );

        if ( ( pixelLocation.x == int( imageStatParms.pickLocation.x ) ) && ( pixelLocation.y == int( imageStatParms.pickLocation.y ) ) )
        {
            // Picked-pixel message [0 - 5]
            imageStatBuffer[ 0 ] = asuint( pixel.r );
            imageStatBuffer[ 1 ] = asuint( pixel.g );
            imageStatBuffer[ 2 ] = asuint( pixel.b );
            imageStatBuffer[ 3 ] = asuint( pixel.a );
            imageStatBuffer[ 4 ] = uint( pixelLocation.x );
            imageStatBuffer[ 5 ] = uint( pixelLocation.y );
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // One thread per group flushes group min/max to the global buffer.
    if ( groupId == 0 )
    {
        // Min/Max message [6 - 13]
        InterlockedMin( imageStatBuffer[ 6 ], groupMinSample[ 0 ] );
        InterlockedMin( imageStatBuffer[ 7 ], groupMinSample[ 1 ] );
        InterlockedMin( imageStatBuffer[ 8 ], groupMinSample[ 2 ] );
        InterlockedMin( imageStatBuffer[ 9 ], groupMinSample[ 3 ] );

        InterlockedMax( imageStatBuffer[ 10 ], groupMaxSample[ 0 ] );
        InterlockedMax( imageStatBuffer[ 11 ], groupMaxSample[ 1 ] );
        InterlockedMax( imageStatBuffer[ 12 ], groupMaxSample[ 2 ] );
        InterlockedMax( imageStatBuffer[ 13 ], groupMaxSample[ 3 ] );
    }

    // Flush per-group histogram bins to the global buffer.
    if ( groupId < HistogramBins ) {
    //    InterlockedAdd( imageStatBuffer[ imageStatParms.baseOffset + groupId ], groupHistogramBins[ groupId ] );
    }
}
