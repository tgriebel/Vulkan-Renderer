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

struct ImageViewer
{
	float4	scissorRectUv;
	uint	flags;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageViewer )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;

	const bool isCubeImage = ( ( imageProcess.flags >> 0 ) & 1 ) != 0;

	float2 uv = ( input.fragTexCoord.xy - imageProcess.scissorRectUv.xy ) / imageProcess.scissorRectUv.zw;

	if( isCubeImage ) {
		output.outColor = codeCubeSamplers[NUI( 0 )].Sample( codeCubeSamplersSt, float3( uv, 0.0f ) );
	} else {
		output.outColor = codeSamplers[NUI( 0 )].Sample( codeSamplersSt, uv );
	}

	return output;
}
