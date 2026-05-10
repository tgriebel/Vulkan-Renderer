// DoF tile min/max CoC reduction (compute)

#include "globals.h"
#include "util.h"

#define TILE_SIZE       8
#define TILE_THREADS    ( TILE_SIZE * TILE_SIZE )

struct DofTileParms
{
    float4 dimensions;  // xy = output tile-grid size in tiles
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
CONSTANT_LAYOUT( 0, 2, DofTileParms, dofTileParms )
WRITE_IMAGE_LAYOUT( 0, 3, RWTexture2D<float4>, dofTileOut )

groupshared float g_far [ TILE_THREADS ];
groupshared float g_near[ TILE_THREADS ];

[numthreads( TILE_SIZE, TILE_SIZE, 1 )]
void CSMain( uint3 dtid : SV_DispatchThreadID, uint3 gid : SV_GroupID, uint gi : SV_GroupIndex )
{
    const float4 c = localTextures[ 0 ].Load( int3( dtid.xy, 0 ) );

    g_far [ gi ] = c.r;
    g_near[ gi ] = c.b;
    GroupMemoryBarrierWithGroupSync();

    // Tree reduction across the 64 threads in the group.
    [unroll]
    for ( uint stride = TILE_THREADS / 2; stride > 0; stride >>= 1 )
    {
        if ( gi < stride )
        {
            g_far [ gi ] = max( g_far [ gi ], g_far [ gi + stride ] );
            g_near[ gi ] = max( g_near[ gi ], g_near[ gi + stride ] );
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if ( gi == 0 )
    {
        const uint2 tileGrid = uint2( dofTileParms.dimensions.xy );
        if ( gid.x < tileGrid.x && gid.y < tileGrid.y )
        {
            dofTileOut[ gid.xy ] = float4( g_far[ 0 ], 0.0f, g_near[ 0 ], 1.0f );
        }
    }
    dofTileOut[ gid.xy ] = float4( 1.0f, 0.0f, 1.0f, 1.0f );
}
