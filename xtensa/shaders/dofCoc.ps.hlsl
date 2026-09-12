// Depth-of-Field Circle-of-Confusion
// http://advances.realtimerendering.com/s2014/index.html, CoD:AW slide 100

#include "globals.h"
#include "util.h"

struct dofShaderConstants_t
{
    float4  srcDepthDimensions;
    uint    viewId;
    float   focalLength;
    float   focalPlaneDistance;
    float   apertureDiameter;
    float   maxCocRadius;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, dofShaderConstants_t )

// Commented out functions are from the CoD:AW DoF calculation
//float2 DepthCmp2( float depth, float closestDepth )
//{
//    float d = imageProcess.depthScaleForeground * ( depth - closestDepth );
//    float2 depthCmp;
//    depthCmp.x = smoothstep( 0.0f, 1.0f, d ); // Background
//    depthCmp.y = 1.0f - depthCmp.x; // Foreground
//    return depthCmp;
//}


//float SampleAlpha( float sampleCoc )
//{
//    return min( rcp( PI * sampleCoc * sampleCoc ), rcp( PI * imageProcess.pixelRadius * imageProcess.pixelRadius ) );
//}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 uv = input.uv0.xy;

    psOutput_t output = (psOutput_t)0;

    const float depthSample = localTextures[ 1 ].SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;

    const float4x4 invProj = views[ imageProcess.viewId ].invProjMat;

    const float apertureDiameter = imageProcess.apertureDiameter;
    const float focalLength = imageProcess.focalLength;
    const float focalPlaneDistance = imageProcess.focalPlaneDistance;
    
    const float halfFovX = invProj[ 0 ][ 0 ]; // (i.e. tanHalfAngle)
    const float sensorWidth = 2.0f * focalLength * halfFovX;
    const float toPixelUnitsTransform = ( imageProcess.srcDepthDimensions.x / sensorWidth );

    // CoC is positive, depth needs to increase from camera so negate it      
    const float linearDepth = -LinearDepth( depthSample, invProj );
    float coc = CircleOfConfusion( apertureDiameter, focalLength, focalPlaneDistance, linearDepth );

    coc *= toPixelUnitsTransform;
    coc = clamp( coc, -imageProcess.maxCocRadius, imageProcess.maxCocRadius );
    
    output.outColor = float4( coc, 0.0f, 0.0f, 1.0f );
    return output;
}
