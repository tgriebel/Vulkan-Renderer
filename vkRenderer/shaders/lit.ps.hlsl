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
#include "light_hlsl.h"
#include "color_hlsl.h"
#include "util_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )
PS_LAYOUT_MRT_1_OUT

PS_Output_MRT PSMain( PS_Input input )
{
    PS_Output_MRT output = (PS_Output_MRT)0;

    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = views[ viewlId ];
    const material_t material = materials[ materialId ];

    const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );
    const uint albedoTexId = ( isTextured && material.textureId0 >= 0 ) ? material.textureId0 : globals.defaultAlbedoId;
    const uint normalTexId = ( isTextured && material.textureId1 >= 0 ) ? material.textureId1 : globals.defaultNormalId;
    //const uint normalTexId = globals.defaultNormalId;
    const uint roughnessTexId = ( isTextured && material.textureId2 >= 0 ) ? material.textureId2 : globals.defaultRoughnessId;
    const uint metalnessTexId = ( isTextured && material.textureId3 >= 0 ) ? material.textureId3 : globals.defaultMetalId;

    const float3 diffuseColor = material.Kd.rgb;
    const float3 specularColor = material.Ks.rgb;
    const float specularPower = material.Ns;

    const float4x4 modelMat = surfaces[ input.objectId ].model;
    const float4x4 viewMat = view.viewMat;
    // Extract the transposed upper-left 3x3 manually to avoid DXC transpose()
    // ambiguity with column-major buffer matrices (-fvk-use-gl-layout).
    // Each MatCol3 extracts a column of viewMat, which becomes a row of invViewMat.
    const float3x3 invViewMat = float3x3( MatCol3( viewMat, 0 ),
                                           MatCol3( viewMat, 1 ),
                                           MatCol3( viewMat, 2 ) );
    const float3 cameraOrigin = -mul( invViewMat, float3( viewMat[0][3], viewMat[1][3], viewMat[2][3] ) );
    const float3 modelOrigin = float3( modelMat[0][3], modelMat[1][3], modelMat[2][3] );

    const float4 albedoTex = SrgbToLinear( texSampler[NUI(albedoTexId)].Sample( texSamplerSt, input.uv0.xy ) );
    const float3 normalTex = 2.0f * texSampler[NUI(normalTexId)].Sample( texSamplerSt, input.uv0.xy ).rgb - float3( 1.0f, 1.0f, 1.0f );
    const float4 roughnessTex = texSampler[NUI(roughnessTexId)].Sample( texSamplerSt, input.uv0.xy );
    const float4 metalnessTex = texSampler[NUI(metalnessTexId)].Sample( texSamplerSt, input.uv0.xy );

    const float perceptualRoughness = saturate( globals.generic.x * roughnessTex.r + globals.generic.y );

    const float blendFactor = 1.0f;
    const float3 normal = mul( lerp( float3( 0.0f, 0.0f, 1.0f ), normalize( normalTex ), blendFactor ), float3x3( input.tangent, input.bitangent, input.TBN2 ) );

    const float3 V = normalize( cameraOrigin.xyz - input.worldPosition.xyz );
    const float3 N = normalize( normal ); // normalize( input.worldPosition.xyz - modelOrigin );
    const float3 viewDiffuse = dot( V, N ).xxx;

    const uint diffuseIBL = surfaces[ input.objectId ].diffuseIblCubeId;
    const uint specularIBL = surfaces[ input.objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;

    const int MaxReflectionLod = 4;

    float NoV = max( dot( N, V ), 0.0f );

    const float metallic = saturate( globals.generic.z * metalnessTex.r + globals.generic.w );

    //const float AMBIENT_LIGHT_FACTOR = 0.03f;
    const float ao = 1.0f;

    const float3 albedoColor = albedoTex.rgb * diffuseColor;

    float3 F0 = float3( 0.04f, 0.04f, 0.04f );
    F0 = lerp( F0, albedoColor.rgb, metallic );

    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

#if 1
    for( int i = 0; i < (int)view.numLights; ++i )
    {
        const light_t light = lights[ i ];

        const float3 lightRay = ( light.lightPos.xyz - input.worldPosition.xyz );
        const float lightDistance = length( lightRay );
        const float3 L = lightRay / lightDistance;
        const float3 H = normalize( V + L );

        const float NoL = max( dot( N, L ), 0.0f );
        const float NoH = max( dot( N, H ), 0.0f );
        const float LoH = max( dot( L, H ), 0.0f );
        const float HoV = max( dot( H, V ), 0.0f );

        const float D   = D_GGX( NoH, perceptualRoughness );
        const float G   = G_Smith( NoV, NoL, perceptualRoughness );
        const float3 F  = F_Schlick( HoV, F0 );

        const float3 kS = F;
        float3 kD = float3( 1.0f, 1.0f, 1.0f ) - kS;
        kD *= 1.0f - metallic;

        float3 numerator      = D * G * F;
        float denominator     = 4.0f * NoV * NoL + 0.0001f;
        float3 Fr             = numerator / denominator;

        const float attenuation = 1.0f / ( lightDistance * lightDistance );
        const float spotFalloff = 1.0f;
        const float3 radiance   = attenuation * spotFalloff * light.intensity.rgb;

        float shadowing = 1.0f;
        const uint shadowViewId = light.shadowViewId;
        if ( shadowViewId != 0xFF )
        {
            const view_t shadowView = views[ shadowViewId ];

            const float shadowBias = 0.001f;

            const uint shadowMapTexId = shadowViewId;
            float4 lsPosition = mul( mul( shadowView.projMat, shadowView.viewMat ), float4( input.worldPosition.xyz, 1.0f ) );
            lsPosition.xyz /= lsPosition.w;

            lsPosition.z -= shadowBias;

            const float2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );

            if ( length( ndc.xy - float2( 0.5f, 0.5f ) ) < 0.5f )
            {
                const float shadowMapSample = codeSamplers[NUI(shadowMapTexId)].Sample( codeSamplersSt, ndc.xy ).r;

                shadowing = ( lsPosition.z < shadowMapSample ) ? globals.shadowParms.w : 0.0f; // Assumes spot-light
            } else {
                shadowing = 0.0f; // Assumes spot-light, should be 0.0f for normal lights
            }
        }

        float3 diffuse = ( ( kD * albedoColor.rgb ) / PI + Fr ) * radiance * NoL;

        Lo += shadowing * diffuse;
    }
#endif

    const float3 F = F_SchlickRoughness( NoV, F0, perceptualRoughness );

    const float3 R = reflect( -V, N );
    const int MipLevels = min( (int)GetTextureLevelsCube( cubeSamplers[NUI(specularIBL)] ), MaxReflectionLod );
    const float3 specIBL = cubeSamplers[NUI(specularIBL)].SampleLevel( cubeSamplersSt, CubeVector( R ), perceptualRoughness * MipLevels ).rgb;

    const float2 envBRDF = texSampler[NUI(brdfLutId)].Sample( texSamplerSt, float2( NoV, perceptualRoughness ) ).rg;
    const float3 specular = specIBL * ( F * envBRDF.x + envBRDF.y );

    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const float3 irradiance = cubeSamplers[NUI(diffuseIBL)].Sample( cubeSamplersSt, CubeVector( N ) ).rgb;
    const float3 diffuse = irradiance * albedoColor;
    const float3 ambient = ( kD * diffuse + specular ) * ao;// * material.Ka.rgb;

    output.outColor.rgb = Lo + ambient;
    output.outColor.a = material.Tr;

    output.outColor1.rgb = 0.5f * ( N + float3( 1.0f, 1.0f, 1.0f ) );
    //output.outColor1.rgb = float3( input.uv0.xy, 0.0f );
    output.outColor1.a = 1.0f;

    //output.outColor.rgb = 0.5f * normalTex + float3( 0.5f, 0.5f, 0.5f );
    //output.outColor.rg = envBRDF.rg;//float3( NoV );

//    output.outColor.rgb = float3( NoV, NoV, NoV );
//  output.outColor.rgb += float3( 1.0f, 0.0f, 0.0f ) * pow( 1.0f - NoV, 2.0f );
//  output.outColor.rgb = envColor.rgb;
//  output.outColor.rgb = 0.5f * N + float3( 0.5f, 0.5f, 0.5f );
//  output.outColor.rg = input.uv0.rb;
    return output;
}
