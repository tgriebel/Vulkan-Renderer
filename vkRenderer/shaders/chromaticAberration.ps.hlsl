#include "globals.h"

struct chromaticAberration_t
{
    float4 padding;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, chromaticAberration_t )


float3 ApplyChromaticAberration( Texture2D sceneTexture, const float3 sceneColor, const float2 uv )
{
    if ( globals.chromaticAberration.x != 0.0f )
    {
        const float intensity = globals.chromaticAberration.y;

        const float2 dir = ( uv - 0.5f );
        const float  dist = length( dir );
        const float2 offset = dir * dist * intensity;

        const float r = sceneTexture.Sample( bilinearSamplerClampEdge, uv + offset ).r;
        const float g = sceneColor.g;
        const float b = sceneTexture.Sample( bilinearSamplerClampEdge, uv - offset ).b;

        return float3( r, g, b );
    }

    return sceneColor;
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const float2 uv = input.uv0.xy;
    Texture2D sceneTexture = localTextures[ 0 ];

    const float3 sceneColor = sceneTexture.Sample( bilinearSamplerClampEdge, uv ).rgb;

    output.outColor = float4( ApplyChromaticAberration( sceneTexture, sceneColor, uv ), 1.0f );
    return output;
}
