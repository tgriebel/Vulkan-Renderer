#include "globals.h"

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageShaderTask )

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    // Source: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float filterRadius = 0.005f;
    float x = filterRadius;
    float y = filterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - x, input.uv0.y + y ) ).rgb;
    float3 b = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x,     input.uv0.y + y ) ).rgb;
    float3 c = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + x, input.uv0.y + y ) ).rgb;

    float3 d = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - x, input.uv0.y ) ).rgb;
    float3 e = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x,     input.uv0.y ) ).rgb;
    float3 f = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + x, input.uv0.y ) ).rgb;

    float3 g = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x - x, input.uv0.y - y ) ).rgb;
    float3 h = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x,     input.uv0.y - y ) ).rgb;
    float3 i = localTextures[ 0 ].Sample( bilinearSamplerClampEdge, float2( input.uv0.x + x, input.uv0.y - y ) ).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    output.outColor.rgb = e * 4.0f;
    output.outColor.rgb += ( b + d + f + h ) * 2.0f;
    output.outColor.rgb += ( a + c + g + i );
    output.outColor.rgb *= 1.0f / 16.0f;
    output.outColor.a = 1.0f;

    return output;
}
