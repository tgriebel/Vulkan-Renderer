#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_STANDARD

void main()
{
	objectId = pushConstants.objectId;

	const float maxHeight = 1.0f;
	vec3 position = inPosition;
	worldPosition = ubo[ objectId ].model * vec4( position, 1.0f );
    gl_Position = ubo[ objectId ].proj * ubo[ objectId ].view * worldPosition;
	gl_Position.z = 0.0f;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	fragNormal = inNormal;
	clipPosition = gl_Position;
}