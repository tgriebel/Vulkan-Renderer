// DoF tile min/max CoC reduction (compute)

#include "globals.h"
#include "util.h"

#define TILE_SIZE_X       16
#define TILE_SIZE_Y       16
#define TILE_THREADS    ( TILE_SIZE_X * TILE_SIZE_Y )

struct dofTileConstants_t
{
    float4  srcDepthDimensions;
    uint    viewId;
    float   focalLength;
    float   focalPlaneDistance;
    float   apertureDiameter;
    float   maxCocRadius;
};

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
CODE_IMAGE_LAYOUT( 0, 2, Texture2D )
WRITE_IMAGE_LAYOUT( 0, 3, RWTexture2D<float4>, dofTileOut )

BIND_INLINE dofTileConstants_t dofTileParms;

groupshared uint groupMinSample;
groupshared uint groupMaxSample;
groupshared uint groupMaxCocSample;

[numthreads( TILE_SIZE_X, TILE_SIZE_Y, 1 )]
void CSMain( uint3 threadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex )
{
    const uint2 dimensions = uint2( dofTileParms.srcDepthDimensions.xy );
    
    const bool inBounds = ( threadId.x < dimensions.x ) && ( threadId.y < dimensions.y );
    
    if ( groupIndex == 0 )
    {
        groupMinSample = asuint( 1.0f );
        groupMaxSample = 0;
        groupMaxCocSample = asuint( 0.0f );
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    const float4x4 invProj = views[ dofTileParms.viewId ].invProjMat;

    const float apertureDiameter = dofTileParms.apertureDiameter / 1000.0f;
    const float focalLength = dofTileParms.focalLength / 1000.0f;
    const float focalPlaneDistance = dofTileParms.focalPlaneDistance / 1000.0f;
    
    if( inBounds )
    {
        // Depth is [0, 1] so it's fine just to cast the bits as a uint for min/max
        const float depthSample = localTextures[ 0 ].Load( int3( threadId.xy, 0 ) ).r;
        const uint depthUint = asuint( depthSample );
        
        // CoC is positive, depth needs to increase from camera so negate it      
        const float linearDepth = -LinearDepth( depthSample, invProj );
        const float coc = CircleOfConfusion( apertureDiameter, focalLength, focalPlaneDistance, linearDepth );
        const uint cocUint = asuint( coc );
    
        InterlockedMin( groupMinSample, depthUint );
        InterlockedMax( groupMaxSample, depthUint );
        InterlockedMax( groupMaxCocSample, cocUint );
    }
    
    GroupMemoryBarrierWithGroupSync();

    const float minDepthTile = asfloat( groupMinSample  );
    const float maxDepthTile = asfloat( groupMaxSample );
    const float maxCocTile = min( dofTileParms.maxCocRadius / 1000.0f, asfloat( groupMaxCocSample ) );

    const float halfFovX = invProj[ 0 ][ 0 ];
    const float sensorWidth = 36.0f;
    const float maxCocInPixels = maxCocTile * ( dofTileParms.srcDepthDimensions.x / sensorWidth );
    
    if ( groupIndex == 0 ) {
        dofTileOut[ groupId.xy ] = float4( minDepthTile, maxDepthTile, maxCocInPixels, 1.0f );
    }
}
