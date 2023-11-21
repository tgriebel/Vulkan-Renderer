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
#include "light.h"
#include "color.h"

PS_LAYOUT_STANDARD( sampler2D )
PS_LAYOUT_MRT_1_OUT

void main()
{
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = viewUbo.views[ viewlId ];
    const material_t material = materialUbo.materials[ materialId ];

	const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );
    const uint albedoTexId = isTextured ? material.textureId0 : globals.defaultAlbedoId;
    const uint normalTexId = isTextured ? material.textureId1 : globals.defaultNormalId;
    const uint roughnessTexId = isTextured ? material.textureId2 : globals.defaultRoughnessId;
    const uint metalnessTexId = isTextured ? material.textureId3 : globals.defaultMetalId;
	
	const vec3 diffuseColor = material.Kd.rgb;
    const vec3 specularColor = material.Ks.rgb;
    const float specularPower = material.Ns;

    const mat4 modelMat = ubo.model[ objectId ];
    const mat4 viewMat = view.viewMat;
    const mat3 invViewMat = mat3( transpose( viewMat ) );
    const vec3 cameraOrigin = -invViewMat * vec3( viewMat[ 3 ][ 0 ], viewMat[ 3 ][ 1 ], viewMat[ 3 ][ 2 ] );
    const vec3 modelOrigin = vec3( modelMat[ 3 ][ 0 ], modelMat[ 3 ][ 1 ], modelMat[ 3 ][ 2 ] );

    const vec4 albedoTex = SrgbToLinear( texture( texSampler[ albedoTexId ], fragTexCoord.xy ) );
    const vec3 normalTex = 2.0f * texture( texSampler[ normalTexId ], fragTexCoord.xy ).rgb - vec3( 1.0f, 1.0f, 1.0f );
    const vec4 roughnessTex = texture( texSampler[ roughnessTexId ], fragTexCoord.xy );
    const vec4 metalnessTex = texture( texSampler[ metalnessTexId ], fragTexCoord.xy );

    const float perceptualRoughness = globals.generic.y * roughnessTex.r;

    const float blendFactor = 0.0f;
    const vec3 normal = fragTangentBasis * mix( vec3( 0.0f, 0.0f, 1.0f ), normalTex, blendFactor );

    const vec3 v = normalize( cameraOrigin.xyz - worldPosition.xyz );
    const vec3 n = normalize( normal ); // normalize( worldPosition.xyz - modelOrigin );
    const vec3 viewDiffuse = dot( v, n ).xxx;

    const vec3 r = reflect( -v, n );
    const int MipLevels = textureQueryLevels( cubeSamplers[ 0 ] );
    const vec4 envMap = vec4( SrgbToLinear( textureLod( cubeSamplers[0], vec3( r.x, r.z, r.y ), perceptualRoughness * MipLevels ).rgb ), 1.0f ); // FIXME: HACK

    float NoV = abs( dot( n, v ) );

    float metallic = 0.0f;//metalnessTex.r;
	
	const float AMBIENT_LIGHT_FACTOR = 0.03f;
    const float ao = 1.0f;

	const vec3 albedoColor = albedoTex.rgb;
    const vec3 ambient = ao * albedoColor * AMBIENT_LIGHT_FACTOR * material.Ka.rgb;
	
    vec3 F0 = vec3( 0.04f ); 
    F0 = mix( F0, albedoColor.rgb, metallic );
	
    vec3 Lo = vec3( 0.0f, 0.0f, 0.0f );
    for( int i = 0; i < view.numLights; ++i )
    {
        const light_t light = lightUbo.lights[ i ];

	    const vec3 l = normalize( light.lightPos.xyz - worldPosition.xyz );
        const vec3 h = normalize( v + l );

        const float NoL = max( dot( n, l ), 0.0f );
        const float NoH = max( dot( n, h ), 0.0f );
        const float LoH = max( dot( l, h ), 0.0f );

        const float D   = D_GGX( NoH, perceptualRoughness );
        const float G   = G_Smith( NoV, NoL, perceptualRoughness );      
        const vec3 F    = F_Schlick( NoH, F0 ); 

        const vec3 kS = F;
        vec3 kD = vec3( 1.0f ) - kS;
        kD *= 1.0f - metallic;

        const float spotAngle = dot( l, light.lightDir.xyz );
        const float spotFov = 0.5f;

        vec3 numerator      = D * G * F;
        float denominator   = 4.0f * NoV * NoL + 0.0001;
        vec3 Fr             = numerator / denominator;
               
        const float distance    = length( l );
        const float attenuation = 1.0f / ( distance * distance );
        const float spotFalloff = 1.0f; // * smoothstep( 0.5f, 0.8f, spotAngle );
        const vec3 radiance     = attenuation * spotFalloff * light.intensity.rgb;

        float shadowing = 1.0f;
        const uint shadowViewId = light.shadowViewId;
        if ( shadowViewId != 0xFF )
        {
            const view_t shadowView = viewUbo.views[ shadowViewId ];

            const uint shadowMapTexId = shadowViewId;
            vec4 lsPosition = shadowView.projMat * shadowView.viewMat * vec4( worldPosition.xyz, 1.0f );
            lsPosition.xyz /= lsPosition.w;

            const vec2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
            const float bias = 0.001f;
            const float depth = ( lsPosition.z );

            if ( length( ndc.xy - vec2( 0.5f ) ) < 0.5f )
            {
                const ivec2 shadowPixelLocation = ivec2( globals.shadowParms.yz * ndc.xy );
                const float shadowValue = texelFetch( codeSamplers[ shadowMapTexId ], shadowPixelLocation, 0 ).r;
                if ( shadowValue < ( depth - bias ) ) {
                    shadowing = 1.0f - min( 1.0f, globals.shadowParms.w );
                }
            } else {
                shadowing = 1.0f - min( 1.0f, globals.shadowParms.w );
            }
        }

        vec3 diffuse = ( ( kD * albedoColor.rgb ) / PI + Fr ) * radiance * NoL;

        Lo += shadowing * diffuse;
    }
    outColor.rgb = Lo.rgb + ambient;
    outColor.a = 1.0f;

    outColor1.rgb = 0.5f * ( n + vec3( 1.0f, 1.0f, 1.0f ) );

    //outColor.rgb += vec3( 1.0f, 0.0f, 0.0f ) * pow( 1.0f - NoV, 2.0f );   
	outColor.a = material.Tr;
//  outColor.rgb = envColor.rgb;
//  outColor.rgb = 0.5f * n + vec3( 0.5f, 0.5f, 0.5f );
//  outColor.rg = fragTexCoord.rb;
}