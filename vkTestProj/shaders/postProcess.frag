#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

void main()
{
	const uint objectId = pushConstants.objectId;
    const uint materialId = pushConstants.materialId;

	const uint textureId0 = materials[ materialId ].textureId0;
    const uint textureId1 = materials[ materialId ].textureId1;

	float depth = texture( codeSamplers[ textureId1 ], fragTexCoord ).r;
	vec3 viewColor = texture( codeSamplers[ textureId0 ], fragTexCoord ).rgb;
	outColor = globals.toneMap.rgba * LinearToSrgb( vec4( viewColor.rgb, 1.0 ) );
	//outColor = vec4( 10.0f * depth, 10.0f * depth, 10.0f * depth, 1.0 );
}