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

	vec3 position = inPosition;
	worldPosition = ubo[ objectId ].model * vec4( position, 1.0f );
    gl_Position = ubo[ objectId ].proj * ubo[ objectId ].view * worldPosition;

	// Tangent-space matrix
	{
		const float normalSign = ( floatBitsToUint( inTangent.x ) & 0x1 ) > 0 ? -1.0f : 1.0f;
		vec3 T = normalize( vec3( uintBitsToFloat( floatBitsToUint( inTangent.x ) & ~0x1 ), inTangent.yz ) );
		vec3 N = normalize( normalSign * cross( inTangent, inBitangent ) );
		vec3 B = normalize( inBitangent );
		T = ( ubo[ objectId ].model * vec4( T, 0.0f ) ).xyz;
		N = ( ubo[ objectId ].model * vec4( N, 0.0f ) ).xyz;
		B = ( ubo[ objectId ].model * vec4( B, 0.0f ) ).xyz;
		fragTangentBasis = mat3( T, B, N );
	}

    fragColor = inColor;
    fragTexCoord = inTexCoord;
	clipPosition = gl_Position;
}