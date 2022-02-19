#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_STANDARD

void main()
{
	objectId = pushConstants.objectId + gl_InstanceIndex;
	const uint materialId = pushConstants.materialId;

	vec3 position = inPosition;
	worldPosition = ubo[ objectId ].model * vec4( position, 1.0f );
    gl_Position = ubo[ objectId ].proj * ubo[ objectId ].view * worldPosition;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	//fragNormal = normalize( ubo[ objectId ].model * vec4( cross( inTangent, inBitangent ), 0.0f ) ).xyz;
	fragNormal = normalize( ubo[ objectId ].model * vec4( inNormal, 0.0f ) ).xyz;
	clipPosition = gl_Position;
}