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
	// X: Destination pixel in output MIP
	// s*: Samples from input MIP
	//
	// +-------- +-------- +
	// |         |         |
	// |   s0    |   s1    |
	// |         |         |
	// +---------X---------+
	// |         |         |
	// |   s2    |   s3    |
	// |         |         |
	// +---------+---------+

    const vec2 halfTexel = dimensions.zw * 0.5f; // zw is the reciprocal inverse of the dimensions

    vec3 result = texture( codeSamplers[ 0 ], fragTexCoord.xy ).rgb * 4.0f;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( -halfTexel.x, halfTexel.y ) ).rgb;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( halfTexel.x, halfTexel.y ) ).rgb;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( -halfTexel.x, -halfTexel.y ) ).rgb;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( halfTexel.x, -halfTexel.y ) ).rgb;
    result /= 8.0f;

    outColor = vec4( result, 1.0f );
}