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
#include "util_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const material_t material = materials[ materialId ];
    const view_t view = views[ viewlId ];

#ifdef USE_CUBE_SAMPLER
    const float3 viewVector = normalize( input.objectPosition );
    const float3 skyColor = cubeSamplers[ material.textureId0 ].Sample( cubeSamplersSt, CubeVector( viewVector ) ).rgb;
    output.outColor.rgb = SrgbToLinear( skyColor );
#else
    const float xm = abs( input.normal.x );
    const float ym = abs( input.normal.y );
    const float zm = abs( input.normal.z );
    const float majorAxis = max( max( xm, ym ), zm );

    uint textureId = 0;

    if( majorAxis == xm ) {
        textureId = ( sign( input.normal.x ) > 0.0f ) ? material.textureId0 : material.textureId1;
    } else if( majorAxis == ym ) {
        textureId = ( sign( input.normal.y ) > 0.0f ) ? material.textureId5 : material.textureId4;
    } else if( majorAxis == zm ) {
        textureId = ( sign( input.normal.z ) > 0.0f ) ? material.textureId2 : material.textureId3;
    }
	output.outColor = SrgbToLinear( texSampler[ textureId ].Sample( texSamplerSt, input.uv0.xy ) );
#endif
    output.outColor.a = 1.0f;
	return output;
}
