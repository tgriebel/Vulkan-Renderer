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

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct ImageShaderTask
{
    vec4 generic0;
    vec4 generic1;
    vec4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageShaderTask )

void main()
{
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
    vec3 a = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - x, fragTexCoord.y + y ) ).rgb;
    vec3 b = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x,     fragTexCoord.y + y ) ).rgb;
    vec3 c = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + x, fragTexCoord.y + y ) ).rgb;

    vec3 d = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - x, fragTexCoord.y ) ).rgb;
    vec3 e = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x,     fragTexCoord.y ) ).rgb;
    vec3 f = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + x, fragTexCoord.y ) ).rgb;

    vec3 g = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x - x, fragTexCoord.y - y ) ).rgb;
    vec3 h = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x,     fragTexCoord.y - y ) ).rgb;
    vec3 i = texture( codeSamplers[ 0 ], vec2( fragTexCoord.x + x, fragTexCoord.y - y ) ).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    outColor.rgb = e * 4.0f;
    outColor.rgb += ( b + d + f + h ) * 2.0f;
    outColor.rgb += ( a + c + g + i );
    outColor.rgb *= 1.0f / 16.0f;
    outColor.a = 1.0f;
}