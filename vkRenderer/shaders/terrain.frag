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

PS_LAYOUT_STANDARD( sampler2D )

void main()
{
    const uint materialId = pushConstants.materialId;

    const uint blendId = materialUbo.materials[ materialId ].textureId0;
    const uint textureId0 = materialUbo.materials[ materialId ].textureId1;
    const uint textureId1 = materialUbo.materials[ materialId ].textureId2;

    const float maxHeight = globals.generic.x;
    const vec4 blendValue = maxHeight * texture( texSampler[ blendId ], fragTexCoord.xy );
    const vec4 texColor0 = SrgbToLinear( texture( texSampler[ textureId0 ], fragTexCoord.xy ) );
    const vec4 texColor1 = SrgbToLinear( texture( texSampler[ textureId1 ], fragTexCoord.xy ) );
    const vec4 texColor = mix( texColor1, texColor0, smoothstep( 0.0f, 0.4f, blendValue ) );
    outColor = AMBIENT * texColor;
    for( int i = 0; i < 3; ++i ) {
	    const vec3 lightDist = -normalize( lightUbo.lights[ i ].lightPos.xyz - worldPosition.xyz );
        const float spotAngle = dot( lightDist, lightUbo.lights[ i ].lightDir.xyz );
        const float spotFov = 0.5f;
        vec4 color = texColor;
        // color.rgb *= lights[ i ].intensity * max( 0.0f, dot( lightDist, normalize( fragNormal ) ) );
	    color.rgb *= lightUbo.lights[ i ].intensity.rgb * smoothstep( 0.5f, 0.8f, spotAngle );
        outColor += color;
    }
   // outColor.rgb = fragNormal;

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = int( globals.shadowParms.x );
    vec4 lsPosition = ubo[ shadowId ].proj * ubo[ shadowId ].view * vec4( worldPosition.xyz, 1.0f );
    lsPosition.xyz /= lsPosition.w;
    vec2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
    float bias = 0.0001f;
    float depth = ( lsPosition.z );

    const ivec2 pixelLocation = ivec2( globals.shadowParms.yz * ndc.xy );
    const float shadowValue = texelFetch( codeSamplers[ shadowMapTexId ], pixelLocation, 0 ).r;
    if( shadowValue < ( depth - bias ) ) // shadowed
    {
        visibility = globals.shadowParms.w;
    }
    outColor.rgb *= visibility; 
}