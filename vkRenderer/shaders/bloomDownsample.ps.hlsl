#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageShaderTask )

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

    // Source: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

    float x = dimensions.z;
    float y = dimensions.w;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - 2.0f * x,   input.uv0.y + 2 * y ) ).rgb;
    float3 b = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x,              input.uv0.y + 2 * y ) ).rgb;
    float3 c = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + 2.0f * x,   input.uv0.y + 2 * y ) ).rgb;

    float3 d = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - 2.0f * x,   input.uv0.y ) ).rgb;
    float3 e = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x,              input.uv0.y ) ).rgb;
    float3 f = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + 2.0f * x,   input.uv0.y ) ).rgb;

    float3 g = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - 2.0f * x,   input.uv0.y - 2 * y ) ).rgb;
    float3 h = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x,              input.uv0.y - 2 * y ) ).rgb;
    float3 i = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + 2.0f * x,   input.uv0.y - 2 * y ) ).rgb;

    float3 j = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - x,          input.uv0.y + y ) ).rgb;
    float3 k = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + x,          input.uv0.y + y ) ).rgb;
    float3 l = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - x,          input.uv0.y - y ) ).rgb;
    float3 m = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + x,          input.uv0.y - y ) ).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    output.outColor.rgb = e * 0.125f;
    output.outColor.rgb += ( a + c + g + i ) * 0.03125f;
    output.outColor.rgb += ( b + d + f + h ) * 0.0625f;
    output.outColor.rgb += ( j + k + l + m ) * 0.125f;
    output.outColor.a = 1.0f;

    return output;
}
