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

	const float luminance = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.uv0.xy ).r;
	const float previousLuminance = codeSamplers[NUI( 1 )].Sample( codeSamplersSt, input.uv0.xy ).r;

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
