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

struct DiffuseIblConstants
{
    float4x4 viewMat;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, DiffuseIblConstants )

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

    // GLSL mat4(...) fills columns; HLSL float4x4(...) fills rows.
    // Use transpose() to match GLSL column-major construction.
    float4x4 glslSpace = transpose( float4x4( 0.0f, 0.0f, 1.0f, 0.0f,
                                              -1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f ) );

    const float4x4 viewMat = mul( glslSpace, imageProcess.viewMat );
    const float3 viewForward = -normalize( MatCol3( viewMat, 2 ) );
    const float3 viewRight = normalize( MatCol3( viewMat, 0 ) );
    const float3 viewUp = normalize( MatCol3( viewMat, 1 ) );
    const float3 viewVector = normalize( viewForward + ( 2.0f * input.uv0.x - 1.0f ) * viewRight + ( 2.0f * input.uv0.y - 1.0f ) * viewUp );

    float3 up = float3( 0.0, 1.0, 0.0 );
    float3 right = normalize( cross( up, viewVector ) );
    up = normalize( cross( viewVector, right ) );

    float3 irradiance = float3( 0.0f, 0.0f, 0.0f );

    const float lodBias = -10.0f;

#if 0
    float3 tangentSample = float3( sin( 0.0f ) * cos( 0.0f ), sin( 0.0f ) * sin( 0.0f ), cos( 0.0f ) );
    float3 sampleVec = normalize( tangentSample.x * right + tangentSample.y * up + tangentSample.z * viewVector );
    output.outColor = codeCubeSamplers[ 0 ].SampleBias( codeCubeSamplersSt, sampleVec, lodBias );
    //output.outColor.rgb = 0.5f * ( sampleVec + float3( 1.0f, 1.0f, 1.0f ) );
    output.outColor.a = 1.0f;
#else
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    float sampleDelta = 0.025f;
    float nrSamples = 0.0f;
    for ( float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta )
    {
        for ( float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta )
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3( sin( theta ) * cos( phi ), sin( theta ) * sin( phi ), cos( theta ) );
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * viewVector;
            sampleVec = normalize( sampleVec );

			irradiance += codeCubeSamplers[ 0 ].SampleBias( bilinearSamplerClampEdge, sampleVec, lodBias).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * ( 1.0f / float( nrSamples ) );

    output.outColor = float4( irradiance, 1.0f );
#endif

    return output;
}
