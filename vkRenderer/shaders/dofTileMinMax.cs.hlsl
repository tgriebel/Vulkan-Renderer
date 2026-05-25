// DoF tile min/max CoC reduction (compute)

#include "globals.h"
#include "util.h"

#define TILE_SIZE_X       16
#define TILE_SIZE_Y       16
#define TILE_THREADS    ( TILE_SIZE_X * TILE_SIZE_Y )

struct dofShaderConstants_t
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

BIND_INLINE dofShaderConstants_t dofTileParms;

groupshared uint groupMinDepthSample;
groupshared uint groupMaxDepthSample;
groupshared uint groupMinCocSample;
groupshared uint groupMaxCocSample;
groupshared int groupMinSign;
groupshared int groupMaxSign;

[numthreads( TILE_SIZE_X, TILE_SIZE_Y, 1 )]
void CSMain( uint3 threadId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex )
{
    const uint2 dimensions = uint2( dofTileParms.srcDepthDimensions.xy );
    
    const bool inBounds = ( threadId.x < dimensions.x ) && ( threadId.y < dimensions.y );
    
    if ( groupIndex == 0 )
    {
        groupMinDepthSample = asuint( 1.0f );
        groupMaxDepthSample = 0;
        groupMinCocSample = asuint( 65504.0f );
        groupMaxCocSample = asuint( 0.0f );
        groupMinSign = 1; // 1 is fine since we just care about +/-
        groupMaxSign = -1;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    const float4x4 invProj = views[ dofTileParms.viewId ].invProjMat;

    const float apertureDiameter = dofTileParms.apertureDiameter;
    const float focalLength = dofTileParms.focalLength;
    const float focalPlaneDistance = dofTileParms.focalPlaneDistance;
    
    if( inBounds )
    {
        // Depth is [0, 1] so it's fine just to cast the bits as a uint for min/max
        const float depthSample = localTextures[ 0 ].Load( int3( threadId.xy, 0 ) ).r;
        const uint depthUint = asuint( depthSample );
        
        // CoC is positive, depth needs to increase from camera so negate it      
        const float linearDepth = -LinearDepth( depthSample, invProj );
        const float coc = CircleOfConfusion( apertureDiameter, focalLength, focalPlaneDistance, linearDepth );
        
        const int cocSign = sign( coc );
        
        const uint cocUint = asuint( abs( coc ) );
    
        InterlockedMin( groupMinDepthSample, depthUint );
        InterlockedMax( groupMaxDepthSample, depthUint );
        InterlockedMin( groupMinCocSample, cocUint );
        InterlockedMax( groupMaxCocSample, cocUint );
        InterlockedMin( groupMinSign, cocSign );
        InterlockedMax( groupMaxSign, cocSign );
    }
    
    GroupMemoryBarrierWithGroupSync();

    const float minDepthTile = asfloat( groupMinDepthSample );
    const float maxDepthTile = asfloat( groupMaxDepthSample );

    const float halfFovX = invProj[ 0 ][ 0 ]; // (i.e. tanHalfAngle)
    const float sensorWidth = 2.0f * focalLength * halfFovX;
    const float toPixelUnitsTransform = ( dofTileParms.srcDepthDimensions.x / sensorWidth );

    const float cocClampInPixels = dofTileParms.maxCocRadius;

    float minCocInPixels = asfloat( groupMinCocSample ) * toPixelUnitsTransform;
    minCocInPixels = min( cocClampInPixels, minCocInPixels );
    
    float maxCocInPixels = asfloat( groupMaxCocSample ) * toPixelUnitsTransform;
    maxCocInPixels = min( cocClampInPixels, maxCocInPixels );
    
    if ( groupIndex == 0 ) {
        dofTileOut[ groupId.xy ] = float4( minCocInPixels * sign( groupMinSign ), maxCocInPixels * sign( groupMaxSign ), minDepthTile, maxDepthTile );
    }
}
