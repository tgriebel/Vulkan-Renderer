#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

VS_LAYOUT_BASIC_IO

void main()
{
	objectPosition	= inPosition;
	worldPosition	= vec4( inPosition, 1.0f );
    gl_Position		= worldPosition;
    color		= inColor;
    fragTexCoord	= inTexCoord;
	normal		= inNormal;
	clipPosition	= gl_Position;
}