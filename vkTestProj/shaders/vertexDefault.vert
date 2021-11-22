#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_BASIC_IO

void main()
{
	worldPosition	= vec4( inPosition, 1.0f );
    gl_Position		= worldPosition;
    fragColor		= inColor;
    fragTexCoord	= inTexCoord;
	fragNormal		= normalize( cross( inTangent, inBinorm ) );
	clipPosition	= gl_Position;
}