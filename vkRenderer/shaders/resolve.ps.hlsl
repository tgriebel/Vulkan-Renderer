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

PS_LAYOUT_BASIC_IO
#ifdef USE_MRT
PS_LAYOUT_MRT_1_OUT
#endif

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

#ifdef USE_MSAA
PS_LAYOUT_IMAGE_PROCESS( Texture2DMS<float4>, ImageShaderTask )
#else
PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageShaderTask )
#endif

#ifdef USE_MRT
PS_Output_MRT PSMain( PS_Input input )
#else
PS_Output PSMain( PS_Input input )
#endif
{
    const int2 pixelLocation = int2( dimensions.xy * input.uv0.xy );

    float4 outColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );

    for ( int i = 0; i < int( globals.numSamples ); ++i ) {
#ifdef USE_MSAA
        outColor.rgb += codeSamplers[0].Load( pixelLocation, i ).rgb;
#else
        outColor.rgb += codeSamplers[0].Load( int3( pixelLocation, i ) ).rgb;
#endif
    }
    outColor.rgb /= globals.numSamples;
    outColor.a = 1.0f;

#ifdef USE_MRT
    float4 outColor1 = float4( 0.0f, 0.0f, 0.0f, 1.0f );

    for ( int j = 0; j < int( globals.numSamples ); ++j )
    {
#ifdef USE_MSAA
        outColor1.r += codeSamplers[1].Load( pixelLocation, j ).r;
        outColor1.g += asuint( stencilImage.Load( pixelLocation, j ).r ) == 0x01 ? 1.0f : 0.0f;
#else
        outColor1.r += codeSamplers[1].Load( int3( pixelLocation, j ) ).r;
        outColor1.g += asuint( stencilImage.Load( int3( pixelLocation, j ) ).r ) == 0x01 ? 1.0f : 0.0f;
#endif
    }
    outColor1.rgb /= globals.numSamples;

    PS_Output_MRT output = (PS_Output_MRT)0;

    output.outColor = outColor;
    output.outColor1 = outColor1;
#else
    PS_Output output = (PS_Output)0;

    output.outColor = outColor;
#endif
    return output;
}
