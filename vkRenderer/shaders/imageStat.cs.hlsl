#include "globals.h"
#include "util.h"

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

groupshared uint g_bins[ HistogramBins ];

[numthreads( 16, 16, 1 )]
void CSMain( uint3 dtid : SV_DispatchThreadID, uint gtid : SV_GroupIndex )
{
    if ( gtid < HistogramBins ) {
        g_bins[ gtid ] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    const uint width  = uint( imageStatParms.dimensions.x );
    const uint height = uint( imageStatParms.dimensions.y );
    const int  lod    = int( imageStatParms.dimensions.z );

    if ( ( dtid.x < width ) && ( dtid.y < height ) )
    {
        const float4 pixel = localTextures[ imageStatParms.imageId ].Load( int3( dtid.xy, lod ) );

        // Rec.709 luminance
        const float lum = dot( pixel.rgb, float3( 0.2126f, 0.7152f, 0.0722f ) );

        // Map log2(lum) into [0, HistogramBins) using the configured range.
        const float minLog = imageStatParms.luminanceRange.x;
        const float maxLog = imageStatParms.luminanceRange.y;
        const float l      = log2( max( lum, 1e-6f ) );
        const float t      = saturate( ( l - minLog ) / ( maxLog - minLog ) );
        const uint  bin    = min( uint( t * HistogramBins ), HistogramBins - 1 );

        InterlockedAdd( g_bins[ bin ], 1 );
    }
    GroupMemoryBarrierWithGroupSync();

    // Flush per-group bins to the global buffer.
    if ( gtid < HistogramBins ) {
        InterlockedAdd( imageStatBuffer[ imageStatParms.baseOffset + gtid ], g_bins[ gtid ] );
    }
}
