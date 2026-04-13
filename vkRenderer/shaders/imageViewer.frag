#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct ImageViewer
{
	vec4	scissorRectUv;
	uint	flags;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageViewer )

void main()
{
	const bool isCubeImage = bitfieldExtract( imageProcess.flags, 0, 1 ) != 0;

	vec2 uv = ( fragTexCoord.xy - imageProcess.scissorRectUv.xy ) / imageProcess.scissorRectUv.zw;

	if( isCubeImage ) {
		outColor = texture( codeCubeSamplers[ 0 ], vec3( uv, 0.0f ) );
	} else {
		outColor = texture( codeSamplers[ 0 ], uv );
	}
}