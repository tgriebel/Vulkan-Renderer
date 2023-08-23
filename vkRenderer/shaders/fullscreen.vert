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

VS_LAYOUT_BASIC_IO

vec2 positions[ 3 ] = vec2[] (
	vec2( -1.0f, -1.0f ),
	vec2( 3.0f, -1.0f ),
	vec2( -1.0f, 3.0f )
);

vec2 uvs[ 3 ] = vec2[](
	vec2( 0.0f, 0.0f ),
	vec2( 4.0f, 0.0f ),
	vec2( 0.0f, 4.0f )
);

void main() {
	worldPosition = vec4( positions[ gl_VertexIndex ].xy, 0.0, 1.0 );
	gl_Position = worldPosition;
	fragColor = vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	fragTexCoord = vec4( uvs[ gl_VertexIndex ], 0.0, 0.0 );
	fragNormal = vec3( 0.0f, 0.0f, 1.0f );
	clipPosition = gl_Position;
}