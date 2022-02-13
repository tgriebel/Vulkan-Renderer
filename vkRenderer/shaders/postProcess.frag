#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

void main()
{
    const uint materialId = pushConstants.materialId;

	const uint textureId0 = materials[ materialId ].textureId0;
    const uint textureId1 = materials[ materialId ].textureId1;
    const uint textureId2 = materials[ materialId ].textureId2;

	const mat4 viewMat = ubo[ 0 ].view;
	const vec3 forward = -normalize( vec3( viewMat[ 0 ][ 2 ], viewMat[ 1 ][ 2 ], viewMat[ 2 ][ 2 ] ) );
	const vec3 up = normalize( vec3( viewMat[ 0 ][ 0 ], viewMat[ 1 ][ 0 ], viewMat[ 2 ][ 0 ] ) );	
	const vec3 right = normalize( vec3( viewMat[ 0 ][ 1 ], viewMat[ 1 ][ 1 ], viewMat[ 2 ][ 1 ] ) );
	const vec3 viewVector = normalize( forward + fragTexCoord.x * up + fragTexCoord.y * right );

	//ubo[ objectId ].view;

	const ivec2 pixelLocation = ivec2( globals.dimensions.xy * fragTexCoord.xy );

	float stencilCoverage = 0;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( -1, -1 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( 0, -1 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( 1, -1 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( -1, 0 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( 0, 0 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( 1, 0 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( -1, 1 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( 0, 1 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;
	stencilCoverage += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( 1, 1 ), 0 ).r ) == 0x04 ? 1.0f : 0.0f;

	stencilCoverage /= 9.0f;

	const vec4 uvColor = vec4( fragTexCoord.xy, 0.0f, 1.0f );
	const vec4 sceneColor = vec4( texelFetch( codeSamplers[ textureId0 ], pixelLocation, 0 ).rgb, 1.0f );
	const float sceneDepth = texelFetch( codeSamplers[ textureId1 ], pixelLocation, 0 ).r;
	const float skyMask = ( sceneDepth > 0.0f ) ? 1.0f : 0.0f;
	const vec4 skyColor = vec4( texture( cubeSamplers[ textureId0 ], vec3( -viewVector.y, viewVector.z, viewVector.x ) ).rgb, 1.0f );

	outColor.a = 1.0f;
	if( length( fragTexCoord.xy - vec2( 0.5f, 0.5f ) ) < 0.01f ) {
		outColor.rgb = vec3( 0.0f, 0.0f, 0.0f );
	} else if( abs( stencilCoverage - 0.5f ) < 0.5f ) {
		outColor.rgb = globals.toneMap.rgb * mix( LinearToSrgb( sceneColor ).rgb, vec3( 1.0f, 0.0f, 0.0f ), stencilCoverage );
	} else {
		outColor.rgb = globals.toneMap.rgb * LinearToSrgb( sceneColor ).rgb;
	}
}