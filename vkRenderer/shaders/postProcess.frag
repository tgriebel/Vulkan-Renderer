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

void main()
{
    const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = viewUbo.views[ viewlId ];
	const material_t material = materialUbo.materials[ materialId ];

	const uint textureId0 = material.textureId0;
    const uint textureId1 = material.textureId1;
    const uint textureId2 = material.textureId2;

	const mat4 viewMat = view.viewMat;
	const vec3 forward = -normalize( vec3( viewMat[ 0 ][ 2 ], viewMat[ 1 ][ 2 ], viewMat[ 2 ][ 2 ] ) );
	const vec3 up = normalize( vec3( viewMat[ 0 ][ 0 ], viewMat[ 1 ][ 0 ], viewMat[ 2 ][ 0 ] ) );	
	const vec3 right = normalize( vec3( viewMat[ 0 ][ 1 ], viewMat[ 1 ][ 1 ], viewMat[ 2 ][ 1 ] ) );
	const vec3 viewVector = normalize( forward + fragTexCoord.x * up + fragTexCoord.y * right );

	const ivec2 pixelLocation = ivec2( view.dimensions.xy * fragTexCoord.xy );

	float stencilCoverage = 0;
	stencilCoverage += texelFetch( codeSamplers[ textureId1 ], pixelLocation + ivec2( -1, -1 ), 0 ).g;
	stencilCoverage += texelFetch( codeSamplers[ textureId1 ], pixelLocation + ivec2( 1, -1 ), 0 ).g;
	stencilCoverage += texelFetch( codeSamplers[ textureId1 ], pixelLocation + ivec2( -1, 1 ), 0 ).g;
	stencilCoverage += texelFetch( codeSamplers[ textureId1 ], pixelLocation + ivec2( 1, 1 ), 0 ).g;

	stencilCoverage /= 4.0f;

	vec4 sceneColor = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	sceneColor.rgb = LinearToSrgb( texture( codeSamplers[ textureId0 ], fragTexCoord.xy, 0 ).rgb );

	const vec4 uvColor = vec4( fragTexCoord.xy, 0.0f, 1.0f );
	const float sceneDepth = texelFetch( codeSamplers[ textureId1 ], pixelLocation, 0 ).r;
	const float skyMask = ( sceneDepth > 0.0f ) ? 1.0f : 0.0f;
	const vec4 skyColor = vec4( texture( cubeSamplers[ textureId0 ], vec3( -viewVector.y, viewVector.z, viewVector.x ) ).rgb, 1.0f );

	outColor.rgb = vec3( 0.0f );
	const float focalDepth = 0.01f;
	const float focalRange = 0.25f;
	float coc = ( sceneDepth - focalDepth ) / focalRange;
	const int MAX_MIP_LEVELS = 3;
	coc = clamp( coc, -1.0, 1.0f );
	if ( coc < 0.0f ) {
	//	sceneColor.rgb = LinearToSrgb( textureLod( codeSamplers[ textureId0 ], fragTexCoord.xy, int( coc * MAX_MIP_LEVELS ) ).rgb );
	}

	outColor.a = 1.0f;
	if( abs( stencilCoverage - 0.5f ) < 0.5f ) {
		outColor.rgb = globals.toneMap.rgb * mix( sceneColor.rgb, vec3( 0.0f, 1.0f, 0.0f ), stencilCoverage );
	} else {
		outColor.rgb = globals.toneMap.rgb * sceneColor.rgb;
	}
}