/*
* MIT License
*
* Copyright( c ) 2026 Thomas Griebel
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

#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

struct LuminanceConstants
{
	float dummy;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, LuminanceConstants )


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


PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;

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

	const float2 texelSize = 1.0f / float2( GetTextureSize( codeSamplers[NUI( 0 )], 0 ) );

	if( level == 0 ) // Initial luminance computation: WxH resolution -> square resolution
	{
		const float3 sceneColor = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.fragTexCoord.xy ).rgb;
		const float luminance = dot( sceneColor, float3( 0.2126f, 0.7152f, 0.0722f ) );
		output.outColor.r = log( max( luminance + 0.0001f, 0.0f ) );
	}
	else if( level < ( mipCount - 1 ) ) // Averaged luminance computation
	{
		const float s0 = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.fragTexCoord.xy + texelSize * float2( -0.5f, -0.5f ) ).r;
		const float s1 = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.fragTexCoord.xy + texelSize * float2( 0.5f, -0.5f ) ).r;
		const float s2 = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.fragTexCoord.xy + texelSize * float2( -0.5f, 0.5f ) ).r;
		const float s3 = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.fragTexCoord.xy + texelSize * float2( 0.5f, 0.5f ) ).r;

		output.outColor.r = ReinhardWeightedAverage( s0, s1, s2, s3 );
	}
	else // Final luminance computation
	{
		const float s0 = codeSamplers[NUI( 0 )].SampleLevel( codeSamplersSt, float2( 0.5f, 0.5f ) + texelSize * float2( -0.5f, -0.5f ), 0 ).r;
		const float s1 = codeSamplers[NUI( 0 )].SampleLevel( codeSamplersSt, float2( 0.5f, 0.5f ) + texelSize * float2( 0.5f, -0.5f ), 0 ).r;
		const float s2 = codeSamplers[NUI( 0 )].SampleLevel( codeSamplersSt, float2( 0.5f, 0.5f ) + texelSize * float2( -0.5f, 0.5f ), 0 ).r;
		const float s3 = codeSamplers[NUI( 0 )].SampleLevel( codeSamplersSt, float2( 0.5f, 0.5f ) + texelSize * float2( 0.5f, 0.5f ), 0 ).r;

		const float logLuminance = ReinhardWeightedAverage( s0, s1, s2, s3 );
		const float luminance = exp( logLuminance );

		const float previousLuminance = codeSamplers[NUI( 2 )].SampleLevel( codeSamplersSt, float2( 0.5f, 0.5f ), 0 ).r; // 1x1 texture
		const float dtSec = globals.time.w / 1000.0f;
		const float adaptationRate = globals.exposure.y;
		const float weight = 1.0f - exp( -dtSec * adaptationRate );
		output.outColor.r = previousLuminance + ( luminance - previousLuminance ) * weight;
	}
	output.outColor.a = 1.0f;

	return output;
}
