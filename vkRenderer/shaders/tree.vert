#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_STANDARD( sampler2D )

void main()
{
	objectId = pushConstants.objectId + gl_InstanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = viewUbo.views[ viewlId ];

	vec3 position = inPosition;
	objectPosition = position;
	worldPosition = ubo.surface[ objectId ].model * vec4( position, 1.0f );
    gl_Position = view.projMat * view.viewMat * worldPosition;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	fragNormal = inNormal;
	clipPosition = gl_Position;
}