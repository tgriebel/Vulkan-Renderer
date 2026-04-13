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


float ReinhardWeight( float logLuminance )
{
	const float luminance = exp( logLuminance );
	const float whitePoint = 1.0f;
	return luminance / ( whitePoint + luminance );
}

float ReinhardWeightedAverage( float s0, float s1, float s2, float s3 )
{
	const float w0 = ReinhardWeight( s0 );
	const float w1 = ReinhardWeight( s1 );
	const float w2 = ReinhardWeight( s2 );
	const float w3 = ReinhardWeight( s3 );

	const float totalWeight = w0 + w1 + w2 + w3;
	return ( s0 * w0 + s1 * w1 + s2 * w2 + s3 * w3 ) / totalWeight;
	//return ( s0 + s1 + s2 + s3 ) / 4.0f;
}


void main()
{
	// Equation from Reinhard's "Photographic Tone Reproduction for Digital Images"
	// 
	// Shader goes MIP-by-MIP:
	// 1. First MIP just convert HDR color to a square luminance image
	// 2. Intermediate MIPs compute a log-space average
	// 3. The luminance is converted to linear space and combined with the last frame's linear luminance
	// 4. Only the 1x1 final MIP is used for exposure
	// 
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

	const vec2 texelSize = 1.0f / vec2( textureSize( codeSamplers[ 0 ], 0 ) );

	if( level == 0 ) // Initial luminance computation: WxH resolution -> square resolution
	{
		const vec3 sceneColor = texture( codeSamplers[ 0 ], fragTexCoord.xy ).rgb;
		const float luminance = dot( sceneColor, vec3( 0.2126f, 0.7152f, 0.0722f ) );
		outColor.r = log( max( luminance + 0.0001f, 0.0f ) );
	}
	else if( level < ( mipCount - 1 ) ) // Averaged luminance computation 
	{		
		const float s0 = texture( codeSamplers[ 0 ], fragTexCoord.xy + texelSize * vec2( -0.5f, -0.5f ) ).r;
		const float s1 = texture( codeSamplers[ 0 ], fragTexCoord.xy + texelSize * vec2( 0.5f, -0.5f ) ).r;
		const float s2 = texture( codeSamplers[ 0 ], fragTexCoord.xy + texelSize * vec2( -0.5f, 0.5f ) ).r;
		const float s3 = texture( codeSamplers[ 0 ], fragTexCoord.xy + texelSize * vec2( 0.5f, 0.5f ) ).r;

		outColor.r = ReinhardWeightedAverage( s0, s1, s2, s3 );
	}
	else // Final luminance computation
	{
		const float s0 = textureLod( codeSamplers[ 0 ], vec2( 0.5f ) + texelSize * vec2( -0.5f, -0.5f ), 0 ).r;
		const float s1 = textureLod( codeSamplers[ 0 ], vec2( 0.5f ) + texelSize * vec2( 0.5f, -0.5f ), 0 ).r;
		const float s2 = textureLod( codeSamplers[ 0 ], vec2( 0.5f ) + texelSize * vec2( -0.5f, 0.5f ), 0 ).r;
		const float s3 = textureLod( codeSamplers[ 0 ], vec2( 0.5f ) + texelSize * vec2( 0.5f, 0.5f ), 0 ).r;

		const float logLuminance = ReinhardWeightedAverage( s0, s1, s2, s3 );
		const float luminance = exp( logLuminance );

		const float previousLuminance = textureLod( codeSamplers[ 2 ], vec2( 0.5f ), 0 ).r; // 1x1 texture
		const float dtSec = globals.time.w / 1000.0f;
		const float adaptationRate = globals.exposure.y;
		const float weight = 1.0f - exp( -dtSec * adaptationRate );
		outColor.r = previousLuminance + ( luminance - previousLuminance ) * weight;
	}
   outColor.a = 1.0f;
}