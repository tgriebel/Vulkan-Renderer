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
#include "color.h"

PS_LAYOUT_STANDARD( sampler2D )

const vec2 invAtan = vec2( 0.1591f, 0.3183f );
vec2 SampleSphericalMap( vec3 v )
{
	vec2 uv = vec2( atan( v.y, -v.x ), asin( -v.z ) );
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main()
{
	const uint materialId = pushConstants.materialId;
	const material_t material = materialUbo.materials[ materialId ];

	const vec2 uv = SampleSphericalMap( normalize( objectPosition ) );
	const vec3 color = texture( texSampler[ material.textureId0 ], uv ).rgb;

	outColor = vec4( color, 1.0f );
}