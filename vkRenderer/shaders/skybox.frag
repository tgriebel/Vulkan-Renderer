#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

void main()
{
    const uint materialId = pushConstants.materialId;
    
    const uint textureId = materials[ materialId ].textureId0;

	outColor = SrgbTolinear( texture( texSampler[textureId], fragTexCoord.xy ) );
}