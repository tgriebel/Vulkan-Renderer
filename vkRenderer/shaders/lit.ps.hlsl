#include "globals.h"
#include "light.h"
#include "color.h"
#include "util.h"
#include "brdf.h"

PS_LAYOUT_STANDARD( Texture2D )

#ifdef USE_MRT
PS_LAYOUT_MRT_1_OUT
#endif


float3 ApplyLight( const surfaceInput_t surfaceInput, lightingInput_t lightingInput )
{
	const float perceptualRoughness = surfaceInput.roughness;
	const float metallic = surfaceInput.metallic;
	const float3 F0 = surfaceInput.F0;

	const float D = D_GGX( lightingInput.NoH, perceptualRoughness );
	const float G = G_Smith( surfaceInput.NoV, lightingInput.NoL, perceptualRoughness);
	const float3 F = F_Schlick( lightingInput.HoV, F0 );

	const float3 kS = F;
	float3 kD = float3( 1.0f, 1.0f, 1.0f ) - kS;
	kD *= 1.0f - metallic;

	float3 numerator = D * G * F;
	float denominator = 4.0f * surfaceInput.NoV * lightingInput.NoL + 0.0001f;
	float3 Fr = numerator / denominator;

	const float attenuation = 1.0f / ( lightingInput.lightDistance * lightingInput.lightDistance );
	const float spotFalloff = 1.0f;
	const float3 radiance = attenuation * spotFalloff * lightingInput.intensity;

	const float3 Lo = ( ( kD * surfaceInput.albedo ) / PI + Fr ) * radiance * lightingInput.NoL;
	
	return Lo;
}


float ApplyShadow( const uint shadowViewId, float3 worldPosition )
{
	float shadowing = 1.0f; // Assumes spot-light, should be 0.0f for normal lights
	
	if ( shadowViewId != 0xFF )
	{
		const gpuView_t shadowView = views[shadowViewId];
		
		const uint shadowMapTexId = shadowViewId;
		
		Texture2D shadowMap = codeSamplers[ shadowMapTexId ];

		const float shadowBias = 0.001f;
	
		float4 lsPosition = mul( mul( shadowView.projMat, shadowView.viewMat ), float4( worldPosition.xyz, 1.0f ) );
		lsPosition.xyz /= lsPosition.w;

		lsPosition.z -= shadowBias;
		
		const float2 ndc = 0.5f * lsPosition.xy + 0.5f;

		const float spotRadius = 0.3f;
		const bool withinSpotlight = ( length(ndc.xy - float2( 0.5f, 0.5f ) ) < spotRadius ); // Similar to an SDF. Distance from center below a threshold
		
		if ( withinSpotlight )
		{
			const float shadowMapSample = shadowMap.Sample( depthShadowSampler, ndc.xy ).r;

			shadowing = ( lsPosition.z < shadowMapSample ) ? globals.shadowParms.w : 0.0f;
		}
		else
		{
			shadowing = 0.0f;
		}
	}
	return shadowing;
}


float3 ApplyClearcoat(const surfaceInput_t surfaceInput, lightingInput_t lightingInput, const float3 baseColor)
{
    const float NoH = saturate( dot( surfaceInput.ccNormal, lightingInput.H ) );
    const float NoL = saturate( dot( surfaceInput.ccNormal, lightingInput.L ) );

	const float F0 = 0.04f;
	const float Fc = F0 + ( 1.0f - F0 ) * pow( 1.0f - lightingInput.HoV, 5.0f );
	
    const float D = D_GGX( NoH, surfaceInput.ccRoughness );
	const float visibility = V_Kelemen( lightingInput.LoH );
	
    // Clearcoat contribution
	const float clearcoat = D * visibility * Fc * surfaceInput.ccStrength;

	// Energy loss from base: attenuate by Fresnel of clearcoat
	const float attenuation = 1.0f - Fc * surfaceInput.ccStrength;

	return baseColor * attenuation + clearcoat * NoL;
}


float3 Sheen( const float3 sheenColor, const float roughness,
			  const float NoH, const float NoV, const float NoL )
{
    return sheenColor * D_Charlie( NoH, roughness ) * V_Neubelt( NoV, NoL ) * NoL;
}


#ifdef USE_MRT
PS_Output_MRT PSMain( PS_Input input )
#else
PS_Output PSMain( PS_Input input )
#endif
{
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];
	const gpuMaterial_t material = materials[materialId];

    const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );
	const int albedoTexId = material.textureId[ GGX_ALBEDO_MAP_SLOT ];
	const int normalTexId = material.textureId[ GGX_NORMAL_MAP_SLOT ];
	const int roughnessTexId = material.textureId[ GGX_ROUGHNESS_MAP_SLOT ];
	const int metalnessTexId = material.textureId[ GGX_METALLIC_MAP_SLOT ];
	const int aoTexId = material.textureId[ GGX_AO_MAP_SLOT ];
	const int emissiveTexId = material.textureId[GGX_EMISSIVE_MAP_SLOT];
	const int ccTexId = material.textureId[ GGX_CC_MAP_SLOT ];
	const int ccRoughnessTexId = material.textureId[ GGX_CC_ROUGHNESS_MAP_SLOT ];
	const int ccNormalTexId = material.textureId[ GGX_CC_NML_MAP_SLOT ];
	const int sheenColorTexId = material.textureId[ GGX_SHEEN_COLOR_MAP_SLOT ];
	const int sheenRoughnessTexId = material.textureId[ GGX_SHEEN_ROUGHNESS_MAP_SLOT ];
	const int anisotropyTexId = material.textureId[ GGX_ANISOTROPY_MAP_SLOT ];
	const int transmissionTexId = material.textureId[ GGX_TRANSMISSION_MAP_SLOT ];

    const float3 specularColor = material.Ks.rgb;
    const float specularPower = material.Ns;

    const float4x4 modelMat = surfaces[ input.objectId ].model;
    const float4x4 viewMat = view.viewMat;
    const float3 modelOrigin = float3( modelMat[0][3], modelMat[1][3], modelMat[2][3] );

	float3 albedoSample = material.albedo.rgb;
	float3 normalSample = float3( 0.0f, 0.0f, 1.0f );
	float roughnessSample = material.roughness;
	float metalnessSample = material.metalness;
	float3 emissiveSample = material.emissiveStrength * material.Ke;
	float aoSample = 1.0f;
	float ccSample = material.clearcoatWeight;
	float ccRoughnessSample = material.clearcoatRoughness;
	float3 ccNormalSample = float3( 0.0f, 0.0f, 1.0f );
	float3 sheenSample = material.sheenColor;
	float sheenRoughnessSample = material.sheen;
	float anisotropySample = material.anisotropy;
	float transmissionSample = material.transmissionFactor;
	
	float2 uv0 = input.uv0.xy;

	if ( isTextured )
	{
		if ( albedoTexId >= 0 )
		{
			Texture2D albedoTex = texSampler[ albedoTexId ];
			albedoSample *= SrgbToLinear( albedoTex.Sample( bilinearSamplerWrap, uv0 ) ).rgb;
		}

		if ( normalTexId >= 0 )
		{
			Texture2D normalTex = texSampler[ normalTexId ];
			normalSample = DecodeNormal( normalTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
		}

		if ( roughnessTexId >= 0 )
		{
			Texture2D roughnessTex = texSampler[ roughnessTexId ];
			roughnessSample *= roughnessTex.Sample( bilinearSamplerWrap, uv0 ).g;
		}

		if ( metalnessTexId >= 0 )
		{
			Texture2D metalnessTex = texSampler[ metalnessTexId ];
			metalnessSample *= metalnessTex.Sample( bilinearSamplerWrap, uv0 ).b;
		}

		if ( aoTexId >= 0 )
		{
			Texture2D aoTex = texSampler[ aoTexId ];
			aoSample *= aoTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if ( emissiveTexId >= 0 )
		{
			Texture2D emissiveTex = texSampler[ emissiveTexId ];
			emissiveSample *= SrgbToLinear( emissiveTex.Sample( bilinearSamplerWrap, uv0 )  ).rgb;
		}

		if ( ccTexId >= 0 )
		{
			Texture2D ccTex = texSampler[ ccTexId ];
			ccSample *= ccTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if ( ccRoughnessTexId >= 0 )
		{
			Texture2D ccRoughnessTex = texSampler[ ccRoughnessTexId ];
			ccRoughnessSample *= ccRoughnessTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if ( ccNormalTexId >= 0 )
		{
			Texture2D ccNormalTex = texSampler[ ccNormalTexId ];
			ccNormalSample = DecodeNormal( ccNormalTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
		}

		if ( sheenColorTexId >= 0 )
		{
			Texture2D sheenTex = texSampler[ sheenColorTexId ];
			sheenSample = SrgbToLinear( sheenTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
		}

		if ( sheenRoughnessTexId >= 0 )
		{
			Texture2D sheenRoughnessTex = texSampler[ sheenRoughnessTexId ];
			sheenRoughnessSample = sheenRoughnessTex.Sample(bilinearSamplerWrap, uv0).r;
		}

		if ( anisotropyTexId >= 0 )
		{
			Texture2D anisotropyTex = texSampler[ anisotropyTexId ];
			anisotropySample = anisotropyTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if ( transmissionTexId >= 0 )
		{
			Texture2D transmissionTex = texSampler[ transmissionTexId ];
			transmissionSample = transmissionTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}
	}

	surfaceInput_t surfaceInput;

    const float normalBlendFactor = 1.0f;
	const float3 normal = lerp( float3( 0.0f, 0.0f, 1.0f ), ComputeNormalWS( normalSample, input.tangent, input.bitangent, input.TBN2 ), normalBlendFactor );

    const uint diffuseIBL = surfaces[ input.objectId ].diffuseIblCubeId;
    const uint specularIBL = surfaces[ input.objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;

    const int MaxReflectionLod = 4;

	surfaceInput.N = normalize( normal );
	surfaceInput.V = normalize( view.viewOrigin.xyz - input.worldPosition.xyz );
	surfaceInput.NoV = saturate( dot( surfaceInput.N, surfaceInput.V ) );
	surfaceInput.cameraOrigin = view.viewOrigin.xyz;
	surfaceInput.positionWS = input.worldPosition.xyz;
	surfaceInput.albedo = albedoSample;
	surfaceInput.roughness = saturate( globals.generic.x * roughnessSample + globals.generic.y );
	surfaceInput.metallic = saturate( globals.generic.z * metalnessSample + globals.generic.w );
	surfaceInput.emissive = emissiveSample;
	surfaceInput.F0 = lerp( float3( 0.04f, 0.04f, 0.04f ), surfaceInput.albedo.rgb, surfaceInput.metallic );
	surfaceInput.ao = aoSample;
	
	surfaceInput.ccStrength = ccSample;
	surfaceInput.ccRoughness = ccRoughnessSample;
	surfaceInput.ccNormal = normalize( ComputeNormalWS( ccNormalSample, input.tangent, input.bitangent, input.TBN2 ) );
	
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

#if 1
    for( int i = 0; i < (int)view.numLights; ++i )
    {
		const gpuLight_t light = lights[i];

		const lightingInput_t lightingInput = CalculateLightingInput( surfaceInput, light );

		float3 Lo_i = ApplyLight( surfaceInput, lightingInput );

        Lo_i = ApplyClearcoat( surfaceInput, lightingInput, Lo_i );
		
		const float shadowing = ApplyShadow( light.shadowViewId, surfaceInput.positionWS );

        Lo += shadowing * Lo_i;
    }
#endif

    const float3 F = F_SchlickRoughness( surfaceInput.NoV, surfaceInput.F0, surfaceInput.roughness );

	float3 specular = float3( 0.0f, 0.0f, 0.0f );
	if( globals.useSpecularIBL )
	{
		const float3 R = reflect( -surfaceInput.V, surfaceInput.N );
		const int MipLevels = min((int) GetTextureLevelsCube( cubeSamplers[ specularIBL ] ), MaxReflectionLod );
		const float3 specIBL = cubeSamplers[ specularIBL ].SampleLevel( bilinearSamplerWrap, CubeVector( R ), surfaceInput.roughness * MipLevels ).rgb;

		const float2 envBRDF = texSampler[ brdfLutId ].Sample( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ) ).rg;
		specular = specIBL * ( F * envBRDF.x + envBRDF.y );
	}

    float3 kS = F;
    float3 kD = 1.0 - kS;
	kD *= 1.0 - surfaceInput.metallic;

	float3 diffuse = AMBIENT.rgb * surfaceInput.albedo;
	if ( globals.useDiffuseIBL )
	{
		const float3 irradiance = cubeSamplers[diffuseIBL].Sample(bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
		diffuse = irradiance * surfaceInput.albedo;
	}
	const float3 ambient = ( kD * diffuse + specular ) * surfaceInput.ao;

    float4 outColor;
	outColor.rgb = Lo + ambient + surfaceInput.emissive;
    outColor.a = material.opacity;

    //outColor.rgb = 0.5f * normalTex + float3( 0.5f, 0.5f, 0.5f );

//  outColor.rgb = lightingInput.NoV.xxx;
//	outColor.rgba = float4( input.uv0.xy, 0.0f, 1.0f );
//	outColor.rgba = float4( albedoTex.rgb, 1.0f);

#ifdef USE_MRT
    float4 outColor1;
    outColor1.rgb = 0.5f * ( surfaceInput.N + float3( 1.0f, 1.0f, 1.0f ) );
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
