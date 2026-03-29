/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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
    float3 a = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - x, input.uv0.y + y ) ).rgb;
    float3 b = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x,     input.uv0.y + y ) ).rgb;
    float3 c = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + x, input.uv0.y + y ) ).rgb;

    float3 d = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - x, input.uv0.y ) ).rgb;
    float3 e = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x,     input.uv0.y ) ).rgb;
    float3 f = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + x, input.uv0.y ) ).rgb;

    float3 g = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x - x, input.uv0.y - y ) ).rgb;
    float3 h = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x,     input.uv0.y - y ) ).rgb;
    float3 i = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, float2( input.uv0.x + x, input.uv0.y - y ) ).rgb;

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
