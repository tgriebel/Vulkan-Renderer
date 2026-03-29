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
#include "color_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )

static const float2 invAtan = float2( 0.1591f, 0.3183f );
float2 SampleSphericalMap( float3 v )
{
	float2 uv = float2( atan2( v.y, -v.x ), asin( -v.z ) );
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
	const uint materialId = pushConstants.materialId;
	const material_t material = materials[ materialId ];

	const float2 uv = SampleSphericalMap( normalize( input.objectPosition ) );
	const float3 color = texSampler[NUI(material.textureId0)].Sample( texSamplerSt, uv ).rgb;

	output.outColor = float4( color, 1.0f );
	return output;
}
