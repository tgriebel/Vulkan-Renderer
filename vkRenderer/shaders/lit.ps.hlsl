#include "globals_hlsl.h"
#include "light_hlsl.h"
#include "color_hlsl.h"
#include "util_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )

#ifdef USE_MRT
PS_LAYOUT_MRT_1_OUT
#endif

#ifdef USE_MRT
PS_Output_MRT PSMain( PS_Input input )
#else
PS_Output PSMain( PS_Input input )
#endif
{
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = views[ viewlId ];
    const material_t material = materials[ materialId ];

    const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );
	const uint albedoTexId = material.textureId0;
	const uint normalTexId = material.textureId1;
    //const uint normalTexId = globals.defaultNormalId;
	const uint roughnessTexId = material.textureId2;
	const uint metalnessTexId = material.textureId3;

    const float3 diffuseColor = material.Kd.rgb;
    const float3 specularColor = material.Ks.rgb;
    const float specularPower = material.Ns;

    const float4x4 modelMat = surfaces[ input.objectId ].model;
    const float4x4 viewMat = view.viewMat;
    const float3 cameraOrigin = view.viewOrigin;
    const float3 modelOrigin = float3( modelMat[0][3], modelMat[1][3], modelMat[2][3] );

	float4 albedoSample = float4( 1.0f, 1.0f, 1.0f, 1.0f );
	float3 normalSample = float3( 0.0f, 0.0f, 1.0f );
	float roughnessSample = material.roughness;
	float metalnessSample = material.metalness;

	float2 uv0 = input.uv0.xy;
	
	if ( isTextured && albedoTexId >= 0 ) {
		albedoSample = SrgbToLinear( texSampler[ albedoTexId ].Sample(texSamplerSt, uv0 ) );
	}

	if ( isTextured && normalTexId >= 0 ) {
		normalSample = 2.0f * texSampler[ normalTexId ].Sample( texSamplerSt, uv0 ).rgb - float3( 1.0f, 1.0f, 1.0f );
	}

	if ( isTextured && roughnessTexId >= 0 ) {
		roughnessSample = texSampler[ roughnessTexId ].Sample( texSamplerSt, uv0 ).r;
	}

	if ( isTextured && metalnessTexId >= 0 ) {
		metalnessSample = texSampler[ metalnessTexId ].Sample( texSamplerSt, uv0 ).r;
	}

	const float perceptualRoughness = saturate( globals.generic.x * roughnessSample + globals.generic.y );

    const float blendFactor = 1.0f;
	const float3 normal = lerp( float3( 0.0f, 0.0f, 1.0f ), normalize( normalSample.x * input.tangent + normalSample.y * input.bitangent + normalSample.z * input.TBN2), blendFactor );

    const float3 V = normalize( cameraOrigin.xyz - input.worldPosition.xyz );
    const float3 N = normalize( normal ); // normalize( input.worldPosition.xyz - modelOrigin );
    const float3 viewDiffuse = dot( V, N ).xxx;

    const uint diffuseIBL = surfaces[ input.objectId ].diffuseIblCubeId;
    const uint specularIBL = surfaces[ input.objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;

    const int MaxReflectionLod = 4;

    float NoV = saturate( dot( N, V ) );

	const float metallic = saturate( globals.generic.z * metalnessSample + globals.generic.w );

    //const float AMBIENT_LIGHT_FACTOR = 0.03f;
    const float ao = 1.0f;

	const float3 albedoColor = albedoSample.rgb * diffuseColor;

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
                const float shadowMapSample = codeSamplers[shadowMapTexId].Sample( codeSamplersSt, ndc.xy ).r;

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
    const int MipLevels = min( (int)GetTextureLevelsCube( cubeSamplers[specularIBL] ), MaxReflectionLod );
    const float3 specIBL = cubeSamplers[specularIBL].SampleLevel( cubeSamplersSt, CubeVector( R ), perceptualRoughness * MipLevels ).rgb;

    // FIXME: something with the `envBRDF` broke with the HLSL conversion
    // `texSamplerSt` is the wrong sampler to use, this needs a clamp sampler
	const float2 envBRDF = texSampler[brdfLutId].Sample( bilinearSamplerClampEdge, float2(NoV, perceptualRoughness)).rg;
    const float3 specular = specIBL * ( F * envBRDF.x + envBRDF.y );

    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const float3 irradiance = cubeSamplers[diffuseIBL].Sample( cubeSamplersSt, CubeVector( N ) ).rgb;
    const float3 diffuse = irradiance * albedoColor;
    const float3 ambient = ( kD * diffuse + specular ) * ao;// * material.Ka.rgb;

    float4 outColor;
    outColor.rgb = Lo + ambient;
    outColor.a = material.Tr;

    //outColor.rgb = 0.5f * normalTex + float3( 0.5f, 0.5f, 0.5f );
    //outColor.rg = envBRDF.rg;//float3( NoV );

//  outColor.rgb = float3( NoV, NoV, NoV );
//  outColor.rgb += float3( 1.0f, 0.0f, 0.0f ) * pow( 1.0f - NoV, 2.0f );
//  outColor.rgb = envColor.rgb;
//  outColor.rgb = 0.5f * N + float3( 0.5f, 0.5f, 0.5f );
//	outColor.rgba = float4( input.uv0.xy, 0.0f, 1.0f );
//	outColor.rgba = float4( albedoTex.rgb, 1.0f);

#ifdef USE_MRT
    float4 outColor1;
    outColor1.rgb = 0.5f * ( N + float3( 1.0f, 1.0f, 1.0f ) );
    //outColor1.rgb = float3( input.uv0.xy, 0.0f );
    outColor1.a = 1.0f;

    PS_Output_MRT output = (PS_Output_MRT)0;

    output.outColor = outColor;
    output.outColor1 = outColor1;
#else
    PS_Output output = (PS_Output)0;

    output.outColor = outColor;
#endif
    return output;
}
