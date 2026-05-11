// Depth-of-Field Circle-of-Confusion
// http://advances.realtimerendering.com/s2014/index.html, CoD:AW slide 100

#include "globals.h"
#include "util.h"

struct DofCocConstants
{
    float depthScaleForeground;
    float pixelRadius;
    float pad1;
    float pad2;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, DofCocConstants )

float2 DepthCmp2( float depth, float closestDepth )
{
    float d = imageProcess.depthScaleForeground * ( depth - closestDepth );
    float2 depthCmp;
    depthCmp.x = smoothstep( 0.0f, 1.0f, d ); // Background
    depthCmp.y = 1.0f - depthCmp.x; // Foreground
    return depthCmp;
}


float SampleAlpha( float sampleCoc )
{
    return min( rcp( PI * sampleCoc * sampleCoc ), rcp( PI * imageProcess.pixelRadius * imageProcess.pixelRadius ) );
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 uv = input.uv0.xy;

    psOutput_t output = (psOutput_t)0;

    const float zDepth = localTextures[ 1 ].SampleLevel( bilinearSamplerClampEdge, uv, 0 ).r;

    if ( zDepth <= 0.0f )
    {
        output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
        return output;
    }

    const float4x4 invProj = views[ viewId ].invProjMat;
    const float linearZ = -ReconstructViewPos( uv, zDepth, invProj ).z;

    output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
    return output;
}
