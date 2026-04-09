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

struct GaussianProcess
{
    uint dummy;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, GaussianProcess )

static const uint weightCount = 5;
static const float weights[ 5 ] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

    const bool horizontal = ( pass == 0 ) ? true : false;
    const uint texId = ( pass == 0 ) ? 0 : previousImageId;

    const float lod = 0.0f;

    float2 offset = dimensions.zw;
    output.outColor = float4( codeSamplers[ 0 ].SampleLevel( codeSamplersSt, input.uv0.xy, lod ).rgb * weights[ 0 ], 1.0f );

    if ( horizontal )
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            output.outColor.rgb += codeSamplers[ texId ].SampleLevel( codeSamplersSt, input.uv0.xy + float2( offset.x * i, 0.0 ), lod ).rgb * weights[ i ];
            output.outColor.rgb += codeSamplers[ texId ].SampleLevel( codeSamplersSt, input.uv0.xy - float2( offset.x * i, 0.0 ), lod ).rgb * weights[ i ];
        }
    }
    else
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            output.outColor.rgb += codeSamplers[ texId ].SampleLevel( codeSamplersSt, input.uv0.xy + float2( 0.0, offset.y * i ), lod ).rgb * weights[ i ];
            output.outColor.rgb += codeSamplers[ texId ].SampleLevel( codeSamplersSt, input.uv0.xy - float2( 0.0, offset.y * i ), lod ).rgb * weights[ i ];
        }
    }
    output.outColor.a = 1.0f;

    return output;
}
