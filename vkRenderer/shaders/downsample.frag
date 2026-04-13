#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct ImageShaderTask
{
    vec4 generic0;
    vec4 generic1;
    vec4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageShaderTask )

void main()
{
	// X: Destination pixel in output MIP
	// s*: Samples from input MIP
	//
	// +-------- +-------- +
	// |         |         |
	// |   s0    |   s1    |
	// |         |         |
	// +---------X---------+
	// |         |         |
	// |   s2    |   s3    |
	// |         |         |
	// +---------+---------+

    const vec2 halfTexel = dimensions.zw * 0.5f; // zw is the reciprocal inverse of the dimensions

    vec3 result = texture( codeSamplers[ 0 ], fragTexCoord.xy ).rgb * 4.0f;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( -halfTexel.x, halfTexel.y ) ).rgb;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( halfTexel.x, halfTexel.y ) ).rgb;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( -halfTexel.x, -halfTexel.y ) ).rgb;
    result += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( halfTexel.x, -halfTexel.y ) ).rgb;
    result /= 8.0f;

    outColor = vec4( result, 1.0f );
}