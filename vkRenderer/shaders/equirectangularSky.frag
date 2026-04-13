#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"
#include "color.h"

PS_LAYOUT_STANDARD( sampler2D )

const vec2 invAtan = vec2( 0.1591f, 0.3183f );
vec2 SampleSphericalMap( vec3 v )
{
	vec2 uv = vec2( atan( v.y, -v.x ), asin( -v.z ) );
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main()
{
	const uint materialId = pushConstants.materialId;
	const material_t material = materialUbo.materials[ materialId ];

	const vec2 uv = SampleSphericalMap( normalize( objectPosition ) );
	const vec3 color = texture( texSampler[ material.textureId0 ], uv ).rgb;

	outColor = vec4( color, 1.0f );
}