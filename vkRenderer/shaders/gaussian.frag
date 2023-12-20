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

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct ImageProcess
{
    vec4 dimensions;
    vec4 generic0;
    vec4 generic1;
    vec4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, ImageProcess )

const uint weightCount = 5;
const float weights[ weightCount ] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

void main()
{
    const bool horizontal = ( imageProcess.generic0.x != 0.0f ) ? true : false;

    vec2 offset = imageProcess.dimensions.zw;
    outColor = vec4( texture( codeSamplers[ 0 ], fragTexCoord.xy ).rgb * weights[ 0 ], 1.0f );

    if ( horizontal )
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            outColor.rgb += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( offset.x * i, 0.0 ) ).rgb * weights[ i ];
            outColor.rgb += texture( codeSamplers[ 0 ], fragTexCoord.xy - vec2( offset.x * i, 0.0 ) ).rgb * weights[ i ];
        }
    }
    else
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            outColor.rgb += texture( codeSamplers[ 0 ], fragTexCoord.xy + vec2( 0.0, offset.y * i ) ).rgb * weights[ i ];
            outColor.rgb += texture( codeSamplers[ 0 ], fragTexCoord.xy - vec2( 0.0, offset.y * i ) ).rgb * weights[ i ];
        }
    }
}