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
#include "color.h"

PS_LAYOUT_BASIC_IO

struct DiffuseIblConstants
{
    mat4 viewMat;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, DiffuseIblConstants )

void main()
{
    const mat4 viewMat = imageProcess.viewMat;
    const vec3 forward = -normalize( vec3( viewMat[ 0 ][ 2 ], viewMat[ 1 ][ 2 ], viewMat[ 2 ][ 2 ] ) );
    const vec3 up = normalize( vec3( viewMat[ 0 ][ 0 ], viewMat[ 1 ][ 0 ], viewMat[ 2 ][ 0 ] ) );
    const vec3 right = normalize( vec3( viewMat[ 0 ][ 1 ], viewMat[ 1 ][ 1 ], viewMat[ 2 ][ 1 ] ) );
    const vec3 viewVector = normalize( forward + ( 2.0f * fragTexCoord.x - 1.0f ) * up + ( 2.0f * fragTexCoord.y - 1.0f ) * right );

    outColor = vec4( texture( cubeSamplers[ 0 ], vec3( -viewVector.y, viewVector.z, viewVector.x ) ).rgb, 1.0f );
    outColor = LinearToSrgb( outColor );

    /*
    vec3 normal = normalize( objectPosition );

    vec3 irradiance = vec3( 0.0f );

    vec3 up = vec3( 0.0f, 1.0f, 0.0f );
    vec3 right = normalize( cross( up, normal ) );
    up = normalize( cross( normal, right ) );

    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    float sampleDelta = 0.025f;
    float nrSamples = 0.0f;
    for ( float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta )
    {
        for ( float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta )
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3( sin( theta ) * cos( phi ), sin( theta ) * sin( phi ), cos( theta ) );
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture( cubeSamplers[ 0 ], sampleVec ).rgb * cos( theta ) * sin( theta );
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * ( 1.0f / float( nrSamples ) );

	outColor = vec4( irradiance, 1.0f );
    */
}