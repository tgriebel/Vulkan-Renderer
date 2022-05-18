#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_STANDARD( sampler2D )

mat3 GetTerrainTangent( vec2 uv )
{
	ivec2 texDim = textureSize( texSampler[2], 0 );
	
	const float maxHeight = 1.0f;
	float gridSize = 0.01f;
	int cx = int( uv.x * texDim.x );
	int cy = int( uv.y * texDim.y );
	int x0 = cx - 1;
	int x1 = cx + 1;
	int y0 = cy - 1;
	int y1 = cy + 1;

	float dzdx = texelFetch( texSampler[2], ivec2( x1, cy ), 0 ).r - texelFetch( texSampler[2], ivec2( x0, cy ), 0 ).r;
	float dzdy = texelFetch( texSampler[2], ivec2( cx, y1 ), 0 ).r - texelFetch( texSampler[2], ivec2( cx, y0 ), 0 ).r;

	dzdx *= maxHeight / gridSize;
	dzdy *= maxHeight / gridSize;

	vec3 bx = normalize( vec3( 2.0f, 0.0f, dzdx ) );
	vec3 by = normalize( vec3( 0.0f, 2.0f, dzdy ) );

	return mat3( bx, by, vec3( 0.0f, 0.0f, 1.0f ) );
}

void main()
{
	objectId = pushConstants.objectId + gl_InstanceIndex;
	const uint materialId = pushConstants.materialId;

	const uint heightMapId = materials[ materialId ].textureId0;
	const float heightMapValue = texture( texSampler[ heightMapId ], inTexCoord.xy ).r;

	const float maxHeight = globals.generic.x;
	vec3 position = inPosition;
	position.z += maxHeight * heightMapValue;
	worldPosition = ubo[ objectId ].model * vec4( position, 1.0f );
    gl_Position = ubo[ objectId ].proj * ubo[ objectId ].view * worldPosition;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	mat3 worldTangent = ( mat3( ubo[ objectId ].model ) * GetTerrainTangent( inTexCoord.xy ) );
	fragNormal = normalize( cross( worldTangent[0], worldTangent[1] ) );
	clipPosition = gl_Position;
}