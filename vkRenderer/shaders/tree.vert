#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_STANDARD

void main()
{
	const uint objectId = pushConstants.objectId;
	const uint materialId = pushConstants.materialId;

	vec3 position = inPosition;
	worldPosition = ubo[ objectId ].model * vec4( position, 1.0f );
    gl_Position = ubo[ objectId ].proj * ubo[ objectId ].view * worldPosition;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	fragNormal = normalize( cross( inTangent, inBinorm ) );
	clipPosition = gl_Position;
}