// DoF tile min/max CoC reduction (compute)

#include "globals.h"
#include "util.h"

#define TILE_SIZE_X       16
#define TILE_SIZE_Y       16
#define TILE_THREADS    ( TILE_SIZE_X * TILE_SIZE_Y )

struct dofTileConstants_t
{
    float4 dimensions;
};

GLOBALS_LAYOUT( 0, 0 )
CODE_IMAGE_LAYOUT( 0, 1, Texture2D )
WRITE_IMAGE_LAYOUT( 0, 2, RWTexture2D<float4>, dofTileOut )

BIND_INLINE dofTileConstants_t dofTileParms;

groupshared uint groupMinSample;
groupshared uint groupMaxSample;

[numthreads( TILE_SIZE_X, TILE_SIZE_Y, 1 )]
void CSMain( uint3 threadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex )
{
    const uint2 dimensions = uint2( dofTileParms.dimensions.xy );
    
    const bool inBounds = ( threadId.x < dimensions.x ) && ( threadId.y < dimensions.y );
    
    if ( groupIndex == 0 )
    {
        groupMinSample = asuint( 1.0f );
        groupMaxSample = 0;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    if( inBounds )
    {
        // Depth is [0 - 1] so it's fine just to cast the bits as a uint for min/max
        const float depthSample = localTextures[ 0 ].Load( int3( threadId.xy, 0 ) ).r;
        const uint depthUint = asuint( depthSample );
    
        InterlockedMin( groupMinSample, depthUint );
        InterlockedMax( groupMaxSample, depthUint );
    }
    
    GroupMemoryBarrierWithGroupSync();

    const float minDepthTile = asfloat( groupMinSample  );
    const float maxDepthTile = asfloat( groupMaxSample );

    if ( groupIndex == 0 ) {
        dofTileOut[ groupId.xy ] = float4( minDepthTile, maxDepthTile, 0.0f, 1.0f );
    }
}
