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

	const mat4 viewMat = ubo[ 0 ].view;
	const vec3 forward = -normalize( vec3( viewMat[ 0 ][ 2 ], viewMat[ 1 ][ 2 ], viewMat[ 2 ][ 2 ] ) );
	const vec3 up = normalize( vec3( viewMat[ 0 ][ 0 ], viewMat[ 1 ][ 0 ], viewMat[ 2 ][ 0 ] ) );	
	const vec3 right = normalize( vec3( viewMat[ 0 ][ 1 ], viewMat[ 1 ][ 1 ], viewMat[ 2 ][ 1 ] ) );
	const vec3 viewVector = normalize( forward + fragTexCoord.x * up + fragTexCoord.y * right );

	//ubo[ objectId ].view;

	const vec4 uvColor = vec4( fragTexCoord.xy, 0.0f, 1.0f );
	const vec4 sceneColor = vec4( texture( codeSamplers[ textureId0 ], fragTexCoord.xy ).rgb, 1.0f );
	const float sceneDepth = texture( codeSamplers[ textureId1 ], fragTexCoord.xy ).r;
	const float skyMask = ( sceneDepth > 0.0f ) ? 1.0f : 0.0f;
	const vec4 skyColor = vec4( texture( cubeSamplers[ textureId0 ], vec3( -viewVector.y, viewVector.z, viewVector.x ) ).rgb, 1.0f );
	//outColor = mix( skyColor, sceneColor, skyMask );
	//outColor = uvColor;
	outColor = globals.toneMap.rgba * LinearToSrgb( sceneColor );
	//outColor = vec4( 10.0f * vec3( skyMask ).xyz, 1.0 );
}