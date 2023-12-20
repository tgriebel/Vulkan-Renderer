/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
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
#include "color.h"

PS_LAYOUT_BASIC_IO
PS_LAYOUT_MRT_1_OUT

struct ImageProcess
{
	vec4 dimensions;
	vec4 generic0;
	vec4 generic1;
	vec4 generic2;
};

#ifdef USE_MSAA
PS_LAYOUT_IMAGE_PROCESS( sampler2DMS, ImageProcess )
#else
PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageProcess )
#endif

void main()
{
    const ivec2 pixelLocation = ivec2( imageProcess.dimensions.xy * fragTexCoord.xy );

	outColor = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	for ( int i = 0; i < int( globals.numSamples ); ++i ) {
		outColor.rgb += LinearToSrgb( texelFetch( codeSamplers[ 0 ], pixelLocation, i ).rgb );
	}
	outColor.rgb /= globals.numSamples;
	outColor.rgb = SrgbToLinear( outColor.rgb );
	outColor.a = 1.0f;

	outColor1 = vec4( 0.0f, 0.0f, 0.0f, 1.0f );

	for ( int i = 0; i < int( globals.numSamples ); ++i )
	{
		outColor1.r += ( texelFetch( codeSamplers[ 1 ], pixelLocation, i ).r );
		outColor1.g += floatBitsToUint( texelFetch( stencilImage, pixelLocation + ivec2( -1, -1 ), 0 ).r ) == 0x01 ? 1.0f : 0.0f;
	}
	outColor1.rgb /= globals.numSamples;
}