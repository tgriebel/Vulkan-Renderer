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

#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageShaderTask )

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

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

    const float2 halfTexel = dimensions.zw * 0.5f; // zw is the reciprocal inverse of the dimensions

    float3 result = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.uv0.xy ).rgb * 4.0f;
    result += codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.uv0.xy + float2( -halfTexel.x, halfTexel.y ) ).rgb;
    result += codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.uv0.xy + float2( halfTexel.x, halfTexel.y ) ).rgb;
    result += codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.uv0.xy + float2( -halfTexel.x, -halfTexel.y ) ).rgb;
    result += codeSamplers[NUI( 0 )].Sample( codeSamplersSt, input.uv0.xy + float2( halfTexel.x, -halfTexel.y ) ).rgb;
    result /= 8.0f;

    output.outColor = float4( result, 1.0f );

    return output;
}
