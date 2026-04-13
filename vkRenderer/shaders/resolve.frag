#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"
#include "color.h"

PS_LAYOUT_BASIC_IO
PS_LAYOUT_MRT_1_OUT

struct ImageShaderTask
{
	vec4 generic0;
	vec4 generic1;
	vec4 generic2;
};

#ifdef USE_MSAA
PS_LAYOUT_IMAGE_PROCESS( sampler2DMS, ImageShaderTask )
#else
PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageShaderTask )
#endif

void main()
{
    const ivec2 pixelLocation = ivec2( dimensions.xy * fragTexCoord.xy );

	outColor = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	for ( int i = 0; i < int( globals.numSamples ); ++i ) {
		outColor.rgb += texelFetch( codeSamplers[ 0 ], pixelLocation, i ).rgb;
	}
	outColor.rgb /= globals.numSamples;
	outColor.a = 1.0f;

	outColor1 = vec4( 0.0f, 0.0f, 0.0f, 1.0f );

	for ( int i = 0; i < int( globals.numSamples ); ++i )
	{
		outColor1.r += texelFetch( codeSamplers[ 1 ], pixelLocation, i ).r;
		outColor1.g += floatBitsToUint( texelFetch( stencilImage, pixelLocation, i ).r ) == 0x01 ? 1.0f : 0.0f;
	}
	outColor1.rgb /= globals.numSamples;
}