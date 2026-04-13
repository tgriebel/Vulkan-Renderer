#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct LuminanceConstants
{
	float dummy;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, LuminanceConstants )

void main()
{
   const float luminance = texture( codeSamplers[ 0 ], fragTexCoord.xy ).r;
   const float previousLuminance = texture( codeSamplers[ 1 ], fragTexCoord.xy ).r;

   const float dtSec = globals.time.w / 1000.0f;

   const float adaptationRate = globals.toneMapTint.a;
   const float weight = 1.0f - exp( -dtSec * adaptationRate );

   // Pattanaik et al: "Time-Dependent Visual Adaptation For Fast Realistic Image Display"
   const float weightedAvgLum = previousLuminance + ( luminance - previousLuminance ) * weight;

   outColor.r = weightedAvgLum;
   outColor.g = 0.0f;
   outColor.b = 0.0f;
   outColor.a = 1.0f;
}