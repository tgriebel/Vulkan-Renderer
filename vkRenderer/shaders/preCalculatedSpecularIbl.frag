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

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"
#include "color.h"
#include "light.h"

PS_LAYOUT_BASIC_IO

struct SpecularIblConstants
{
    mat4 viewMat;
    float roughness;
};
PS_LAYOUT_IMAGE_PROCESS( sampler2D, SpecularIblConstants )

// https://learnopengl.com/PBR/IBL/Specular-IBL
void main()
{
     mat4 glslSpace = mat4(  0.0f, 0.0f, 1.0f, 0.0f,
                            -1.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 1.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f );

    const mat4 viewMat = glslSpace * imageProcess.viewMat;
    const vec3 viewForward = -normalize( viewMat[ 2 ].xyz );
    const vec3 viewRight = normalize( viewMat[ 0 ].xyz );
    const vec3 viewUp = normalize( viewMat[ 1 ].xyz );
    const vec3 viewVector = normalize( viewForward + ( 2.0f * fragTexCoord.x - 1.0f ) * viewRight + ( 2.0f * fragTexCoord.y - 1.0f ) * viewUp );

    vec3 up = vec3( 0.0, 1.0, 0.0 );
    vec3 right = normalize( cross( up, viewVector ) );
    up = normalize( cross( viewVector, right ) );

#if 0
    vec3 tangentSample = vec3( sin( 0.0f ) * cos( 0.0f ), sin( 0.0f ) * sin( 0.0f ), cos( 0.0f ) );
    vec3 sampleVec = normalize( tangentSample.x * right + tangentSample.y * up + tangentSample.z * viewVector );
    outColor = texture( codeCubeSamplers[ 0 ], sampleVec );
   // outColor.rgb = 0.5f * ( sampleVec + vec3( 1.0f, 1.0f, 1.0f ) );
    outColor.a = 1.0f;
#else
    vec3 N = normalize( viewVector );
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3( 0.0 );
    for ( uint i = 0u; i < SAMPLE_COUNT; ++i )
    {
        vec2 Xi = Hammersley( i, SAMPLE_COUNT );
        vec3 H = ImportanceSampleGGX( Xi, N, imageProcess.roughness );
        vec3 L = normalize( 2.0 * dot( V, H ) * H - V );

        float NdotL = max( dot( N, L ), 0.0 );
        if ( NdotL > 0.0 )
        {
            prefilteredColor += texture( codeCubeSamplers[ 0 ], L ).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    outColor = vec4( prefilteredColor, 1.0 );
#endif
}