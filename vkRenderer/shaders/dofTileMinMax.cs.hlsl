// DoF tile min/max CoC reduction (compute)

#include "globals.h"
#include "util.h"

#define TILE_SIZE_X       8
#define TILE_SIZE_Y       4
#define TILE_THREADS    ( TILE_SIZE * TILE_SIZE )

struct dofTileConstants_t
{
    float4 dimensions;
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
WRITE_IMAGE_LAYOUT( 0, 2, RWTexture2D<float4>, dofTileOut )

BIND_INLINE dofTileConstants_t dofTileParms;

[numthreads( TILE_SIZE_X, TILE_SIZE_Y, 1 )]
void CSMain( uint3 threadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex )
{
    const uint2 tileGrid = uint2( dofTileParms.dimensions.xy );
    if ( ( groupId.x >= tileGrid.x ) || ( groupId.y >= tileGrid.y ) ) {
        return;
    }
    
    const float depthSample = localTextures[ 0 ].Load( int3( threadId.xy, 0 ) ).r;

    const float minDepth = WaveActiveMin( depthSample );
    const float maxDepth = WaveActiveMax( depthSample );

    if ( groupIndex == 0 ) {
        dofTileOut[ groupId.xy ] = float4( minDepth, maxDepth, 0.0f, 1.0f );
    }
}
