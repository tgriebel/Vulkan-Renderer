#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_OUT

vec2 positions[ 4 ] = vec2[](
	vec2( 0.0f, 0.0f ),
	vec2( 1.0f, 0.0f ),
	vec2( 0.0f, 1.0f ),
	vec2( 1.0f, 1.0f )
);

vec2 uvs[ 4 ] = vec2[](
	vec2( 0.0f, 0.0f ),
	vec2( 1.0f, 0.0f ),
	vec2( 0.0f, 1.0f ),
	vec2( 1.0f, 1.0f )
);

void main()
{
    fragTexCoord = vec4( uvs[ gl_VertexIndex ], 0.0, 0.0 );
    gl_Position = vec4( positions[ gl_VertexIndex ], 0.0f, 0.0f );

	objectPosition	= gl_Position.xyz;
	worldPosition	= gl_Position;
    fragColor		= vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	fragNormal		= vec3( 0.0f, 0.0f, 1.0f );
	clipPosition	= gl_Position;
}