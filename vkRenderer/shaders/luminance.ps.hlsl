#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

struct LuminanceConstants
{
	float dummy;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, LuminanceConstants )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;

	const float luminance = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy ).r;
	const float previousLuminance = codeSamplers[ 1 ].Sample( bilinearSamplerClampEdge, input.uv0.xy ).r;

	const float dtSec = globals.time.w / 1000.0f;

	const float adaptationRate = globals.toneMapTint.a;
	const float weight = 1.0f - exp( -dtSec * adaptationRate );

	// Pattanaik et al: "Time-Dependent Visual Adaptation For Fast Realistic Image Display"
	const float weightedAvgLum = previousLuminance + ( luminance - previousLuminance ) * weight;

	output.outColor.r = weightedAvgLum;
	output.outColor.g = 0.0f;
	output.outColor.b = 0.0f;
	output.outColor.a = 1.0f;

	return output;
}
