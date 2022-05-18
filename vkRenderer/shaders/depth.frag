#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD( sampler2D )

void main()
{
    const uint materialId = pushConstants.materialId;

	outColor = vec4( 1.0, 0.0, 0.0, 1.0 );
}