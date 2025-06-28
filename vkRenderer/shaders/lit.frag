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
#include "util.h"

PS_LAYOUT_STANDARD( sampler2D )
PS_LAYOUT_MRT_1_OUT

void main()
{
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = viewUbo.views[ viewlId ];
    const material_t material = materialUbo.materials[ materialId ];

	const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );
    const uint albedoTexId = ( isTextured && material.textureId0 >= 0 ) ? material.textureId0 : globals.defaultAlbedoId;
    const uint normalTexId = ( isTextured && material.textureId1 >= 0 ) ? material.textureId1 : globals.defaultNormalId;
    //const uint normalTexId = globals.defaultNormalId;
    const uint roughnessTexId = ( isTextured && material.textureId2 >= 0 ) ? material.textureId2 : globals.defaultRoughnessId;
    const uint metalnessTexId = ( isTextured && material.textureId3 >= 0 ) ? material.textureId3 : globals.defaultMetalId;
	
	const vec3 diffuseColor = material.Kd.rgb;
    const vec3 specularColor = material.Ks.rgb;
    const float specularPower = material.Ns;

    const mat4 modelMat = ubo.surface[ objectId ].model;
    const mat4 viewMat = view.viewMat;
    const mat3 invViewMat = mat3( transpose( viewMat ) );
    const vec3 cameraOrigin = -invViewMat * vec3( viewMat[ 3 ][ 0 ], viewMat[ 3 ][ 1 ], viewMat[ 3 ][ 2 ] );
    const vec3 modelOrigin = vec3( modelMat[ 3 ][ 0 ], modelMat[ 3 ][ 1 ], modelMat[ 3 ][ 2 ] );

    const vec4 albedoTex = SrgbToLinear( texture( texSampler[ albedoTexId ], fragTexCoord.xy ) );
    const vec3 normalTex = 2.0f * texture( texSampler[ normalTexId ], fragTexCoord.xy ).rgb - vec3( 1.0f, 1.0f, 1.0f );
    const vec4 roughnessTex = texture( texSampler[ roughnessTexId ], fragTexCoord.xy );
    const vec4 metalnessTex = texture( texSampler[ metalnessTexId ], fragTexCoord.xy );

    const float perceptualRoughness = Saturate( globals.generic.x * roughnessTex.r + globals.generic.y );

    const float blendFactor = 1.0f;
    const vec3 normal = fragTangentBasis * mix( vec3( 0.0f, 0.0f, 1.0f ), normalize( normalTex ), blendFactor );

    const vec3 V = normalize( cameraOrigin.xyz - worldPosition.xyz );
    const vec3 N = normalize( normal ); // normalize( worldPosition.xyz - modelOrigin );
    const vec3 viewDiffuse = dot( V, N ).xxx;

    const uint diffuseIBL = ubo.surface[ objectId ].diffuseIblCubeId;
    const uint specularIBL = ubo.surface[ objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;

    const int MaxRreflectionLod = 4;

    float NoV = max( dot( N, V ), 0.0f );

    const float metallic = Saturate( globals.generic.z * metalnessTex.r + globals.generic.w );
	
	//const float AMBIENT_LIGHT_FACTOR = 0.03f;
    const float ao = 1.0f;

    const vec3 albedoColor = albedoTex.rgb * diffuseColor;

    vec3 F0 = vec3( 0.04f ); 
    F0 = mix( F0, albedoColor.rgb, metallic );
	
    vec3 Lo = vec3( 0.0f, 0.0f, 0.0f );

#if 1
    for( int i = 0; i < view.numLights; ++i )
    {
        const light_t light = lightUbo.lights[ i ];

	    const vec3 L = normalize( light.lightPos.xyz - worldPosition.xyz );
        const vec3 H = normalize( V + L );

        const float NoL = max( dot( N, L ), 0.0f );
        const float NoH = max( dot( N, H ), 0.0f );
        const float LoH = max( dot( L, H ), 0.0f );
        const float HoV = max( dot( H, V ), 0.0f );

        const float D   = D_GGX( NoH, perceptualRoughness );
        const float G   = G_Smith( NoV, NoL, perceptualRoughness );      
        const vec3 F    = F_Schlick( HoV, F0 );

        const vec3 kS = F;
        vec3 kD = vec3( 1.0f ) - kS;
        kD *= 1.0f - metallic;

        vec3 numerator      = D * G * F;
        float denominator   = 4.0f * NoV * NoL + 0.0001;
        vec3 Fr             = numerator / denominator;

        const float spotAngle = dot( L, light.lightDir.xyz );
        const float spotFov = 0.5f;
               
        const float distance    = length( L );
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
                const float shadowValue = texture( codeSamplers[ shadowMapTexId ], ndc.xy ).r;
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
#endif

    const vec3 F = F_SchlickRoughness( NoV, F0, perceptualRoughness );

    const vec3 R = reflect( -V, N );
    const int MipLevels = min( textureQueryLevels( cubeSamplers[ specularIBL ] ), MaxRreflectionLod );
    const vec3 specIBL = textureLod( cubeSamplers[ specularIBL ], CubeVector( R ), perceptualRoughness * MipLevels ).rgb;

    const vec2 envBRDF = texture( texSampler[ brdfLutId ], vec2( NoV, perceptualRoughness ) ).rg;
    const vec3 specular = specIBL * ( F * envBRDF.x + envBRDF.y );

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const vec3 irradiance = texture( cubeSamplers[ diffuseIBL ], CubeVector( N ) ).rgb;
    const vec3 diffuse = irradiance * albedoColor;
    const vec3 ambient = ( kD * diffuse + specular ) * ao;// * material.Ka.rgb;

    outColor.rgb = Lo + ambient;
    outColor.a = material.Tr;

    outColor1.rgb = 0.5f * ( N + vec3( 1.0f, 1.0f, 1.0f ) );
    //outColor1.rgb = vec3( fragTexCoord.xy, 0.0f );
    outColor1.a = 1.0f;

    //outColor.rgb = 0.5f * normalTex + vec3( 0.5f, 0.5f, 0.5f );
    //outColor.rg = envBRDF.rg;//vec3( NoV );

//    outColor.rgb = vec3( NoV );
//  outColor.rgb += vec3( 1.0f, 0.0f, 0.0f ) * pow( 1.0f - NoV, 2.0f );   
//  outColor.rgb = envColor.rgb;
//  outColor.rgb = 0.5f * N + vec3( 0.5f, 0.5f, 0.5f );
//  outColor.rg = fragTexCoord.rb;
}