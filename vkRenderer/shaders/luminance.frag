/*
* MIT License
*
* Copyright( c ) 2025 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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