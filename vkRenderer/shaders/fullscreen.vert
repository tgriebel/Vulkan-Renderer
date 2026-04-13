#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_BASIC_IO

vec2 positions[ 3 ] = vec2[] (
	vec2( -1.0f, -1.0f ),
	vec2( 3.0f, -1.0f ),
	vec2( -1.0f, 3.0f )
);

vec2 uvs[ 3 ] = vec2[](
	vec2( 0.0f, 0.0f ),
	vec2( 2.0f, 0.0f ),
	vec2( 0.0f, 2.0f )
);

// TODO: replace with this
// vec2( ( gl_VertexIndex << 1 ) & 2, gl_VertexIndex & 2 );
// vec4( fragTexCoord.xy * 2.0f + -1.0f, 0.0f, 1.0f );

void main() {
	objectPosition = vec3( positions[ gl_VertexIndex ].xy, 0.0f );
	worldPosition = vec4( positions[ gl_VertexIndex ].xy, 0.0, 1.0 );
	gl_Position = worldPosition;
	fragColor = vec4( 1.0f, 1.0f, 1.0f, 1.0f );
	fragTexCoord = vec4( uvs[ gl_VertexIndex ], 0.0, 0.0 );
	fragNormal = vec3( 0.0f, 0.0f, 1.0f );
	clipPosition = gl_Position;
}