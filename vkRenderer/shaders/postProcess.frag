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
#include "light.h"

PS_LAYOUT_STANDARD( sampler2D )


float SampleCoverage( const ivec2 pos, const uint stencilTextureId ) {
	return texelFetch( codeSamplers[ stencilTextureId ], pos, 0 ).g;
}


float EdgeOutline( const ivec2 pixelLocation, const uint stencilTextureId )
{
	const float c00 = SampleCoverage( pixelLocation + ivec2( -1, -1 ), stencilTextureId );
	const float c10 = SampleCoverage( pixelLocation + ivec2( 0, -1 ), stencilTextureId );
	const float c20 = SampleCoverage( pixelLocation + ivec2( 1, -1 ), stencilTextureId );
	const float c01 = SampleCoverage( pixelLocation + ivec2( -1, 0 ), stencilTextureId );
	const float c11 = SampleCoverage( pixelLocation + ivec2( 0, 0 ), stencilTextureId );
	const float c21 = SampleCoverage( pixelLocation + ivec2( 1, 0 ), stencilTextureId );
	const float c02 = SampleCoverage( pixelLocation + ivec2( -1, 1 ), stencilTextureId );
	const float c12 = SampleCoverage( pixelLocation + ivec2( 0, 1 ), stencilTextureId );
	const float c22 = SampleCoverage( pixelLocation + ivec2( 1, 1 ), stencilTextureId );

	const float minCoverage = min( min( min( c00, c10 ), min( c20, c01 ) ),
		min( min( c11, c21 ), min( c02, min( c12, c22 ) ) ) );
	const float maxCoverage = max( max( max( c00, c10 ), max( c20, c01 ) ),
		max( max( c11, c21 ), max( c02, max( c12, c22 ) ) ) );

	const float edge = maxCoverage - minCoverage;

	const float outerStrength = 0.1f;
	const float innerStrength = 0.8f;
	const float stencilCoverage = smoothstep( outerStrength, innerStrength, edge );

	return stencilCoverage;
}


void main()
{
    const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = viewUbo.views[ viewlId ];
	const material_t material = materialUbo.materials[ materialId ];

	const uint textureId0 = material.textureId0;
    const uint textureId1 = material.textureId1;
    const uint textureId2 = material.textureId2;
    const uint textureId3 = material.textureId3;

	const mat4 viewMat = view.viewMat;
	const vec3 forward = -normalize( vec3( viewMat[ 0 ][ 2 ], viewMat[ 1 ][ 2 ], viewMat[ 2 ][ 2 ] ) );
	const vec3 up = normalize( vec3( viewMat[ 0 ][ 0 ], viewMat[ 1 ][ 0 ], viewMat[ 2 ][ 0 ] ) );	
	const vec3 right = normalize( vec3( viewMat[ 0 ][ 1 ], viewMat[ 1 ][ 1 ], viewMat[ 2 ][ 1 ] ) );
	const vec3 viewVector = normalize( forward + fragTexCoord.x * up + fragTexCoord.y * right );

	const ivec2 pixelLocation = ivec2( view.dimensions.xy * fragTexCoord.xy );

	const float stencilCoverage = EdgeOutline( pixelLocation, textureId1 );

	vec4 sceneColor = vec4( 0.0f, 0.0f, 0.0f, 1.0f );

	const vec4 uvColor = vec4( fragTexCoord.xy, 0.0f, 1.0f );
	const float sceneDepth = texelFetch( codeSamplers[ textureId1 ], pixelLocation, 0 ).r;
	const float skyMask = ( sceneDepth > 0.0f ) ? 1.0f : 0.0f;
	const vec4 skyColor = vec4( texture( cubeSamplers[ textureId0 ], vec3( -viewVector.y, viewVector.z, viewVector.x ) ).rgb, 1.0f );

	outColor.rgb = vec3( 0.0f );
	const bool enabled = globals.dof.x != 0.0f;
	const float focalDepth = globals.dof.y;
	const float focalRange = globals.dof.z;
	float coc = ( sceneDepth - focalDepth ) / focalRange;
	const int MAX_MIP_LEVELS = 3;
	coc = clamp( coc, -1.0, 1.0f );

	vec3 hdrColor;
	if ( enabled && ( coc < 0.0f ) ) {
		hdrColor.rgb = textureLod( codeSamplers[ textureId2 ], fragTexCoord.xy, int( -coc * MAX_MIP_LEVELS ) ).rgb;
	} else {
		hdrColor.rgb = texture( codeSamplers[ textureId0 ], fragTexCoord.xy, 0 ).rgb;
	}

	const vec3 tint = globals.toneMapTint.rgb;
	const float maxLod = float( textureQueryLevels( codeSamplers[ textureId3 ] ) - 1 );
	const float luminance = textureLod( codeSamplers[ textureId3 ], vec2( 0.5f, 0.5f ), maxLod ).r;

	const float middleGrey = globals.exposure.x;
	const float reinhardAlpha = clamp( middleGrey, 0.045f, 0.72f ); // Suggested middle-grey range from reinhard paper
	const float exposure = reinhardAlpha / clamp( luminance, 0.005f, 10000.0f );

	const vec3 exposureAdjustedColor = hdrColor * exposure * tint;

	sceneColor.rgb = LinearToSrgb( exposureAdjustedColor );

	outColor.rgb = mix( sceneColor.rgb, vec3( 0.0f, 1.0f, 0.0f ), stencilCoverage );
	outColor.a = 1.0f;
}