#include "globals.h"
#include "light.h"
#include "color.h"
#include "util.h"
#include "brdf.h"

PS_LAYOUT_STANDARD( Texture2D )

#ifdef USE_MRT
PS_LAYOUT_MRT_1_OUT
#endif


float3 DecodeNormal( const float3 normalMapTexel )
{
	return ( 2.0f * normalMapTexel - float3( 1.0f, 1.0f, 1.0f ) );	
}


lightingInput_t CalculateLightingInput( const surfaceInput_t surfaceInput, const gpuLight_t light )
{
	lightingInput_t lightingInput;
	
	lightingInput.lightRay = ( light.lightPos.xyz - surfaceInput.positionWS );
	lightingInput.lightDistance = length( lightingInput.lightRay );
	lightingInput.L = lightingInput.lightRay / lightingInput.lightDistance;
	lightingInput.H = normalize( surfaceInput.V + lightingInput.L );

	lightingInput.NoL = max( dot( surfaceInput.N, lightingInput.L ), 0.0f );
	lightingInput.NoH = max( dot( surfaceInput.N, lightingInput.H ), 0.0f );
	lightingInput.LoH = max( dot( lightingInput.L, lightingInput.H ), 0.0f );
	lightingInput.HoV = max( dot( lightingInput.H, surfaceInput.V ), 0.0f );

	return lightingInput;
}


float3 ApplyLight( const surfaceInput_t surfaceInput, const gpuLight_t light )
{
	const float3 N = surfaceInput.N;
	const float3 V = surfaceInput.V;
	const float NoV = surfaceInput.NoV;
	const float perceptualRoughness = surfaceInput.roughness;
	const float metallic = surfaceInput.metallic;
	const float3 F0 = surfaceInput.F0;
	
	const float3 lightRay = ( light.lightPos.xyz - surfaceInput.positionWS );
	const float lightDistance = length( lightRay );
	const float3 L = lightRay / lightDistance;
	const float3 H = normalize( V + L );

	const float NoL = max( dot( N, L ), 0.0f );
	const float NoH = max( dot( N, H ), 0.0f );
	const float LoH = max( dot( L, H ), 0.0f );
	const float HoV = max( dot( H, V ), 0.0f );

	const float D = D_GGX( NoH, perceptualRoughness );
	const float G = G_Smith( NoV, NoL, perceptualRoughness);
	const float3 F = F_Schlick( HoV, F0 );

	const float3 kS = F;
	float3 kD = float3( 1.0f, 1.0f, 1.0f ) - kS;
	kD *= 1.0f - metallic;

	float3 numerator = D * G * F;
	float denominator = 4.0f * NoV * NoL + 0.0001f;
	float3 Fr = numerator / denominator;

	const float attenuation = 1.0f / ( lightDistance * lightDistance );
	const float spotFalloff = 1.0f;
	const float3 radiance = attenuation * spotFalloff * light.intensity.rgb;

	const float3 diffuse = ( ( kD * surfaceInput.albedo ) / PI + Fr ) * radiance * NoL;
	
	return diffuse;
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


float3 ApplyClearcoat( const surfaceInput_t surfaceInput, lightingInput_t lightingInput, const float3 baseLayerColor )
{
	// Coat specular
	const float Dc = D_GGX( surfaceInput.ccRoughness, lightingInput.NoH );
	const float Vc = V_Kelemen( lightingInput.LoH );
	const float3 Fc = F_Schlick( 0.04f, lightingInput.LoH );
	const float FcMagnitude = surfaceInput.ccStrength * Fc.x;

	const float coatLobe = surfaceInput.ccStrength * Dc * Vc * Fc.x;

	return ( 1.0f - FcMagnitude ) * baseLayerColor + coatLobe;
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
	const uint albedoTexId = material.textureId[ GGX_ALBEDO_MAP_SLOT ];
	const uint normalTexId = material.textureId[ GGX_NORMAL_MAP_SLOT ];
    //const uint normalTexId = globals.defaultNormalId;
	const uint roughnessTexId = material.textureId[ GGX_ROUGHNESS_MAP_SLOT ];
	const uint metalnessTexId = material.textureId[ GGX_METALLIC_MAP_SLOT ];

    const float3 diffuseColor = material.Kd.rgb;
    const float3 specularColor = material.Ks.rgb;
    const float specularPower = material.Ns;

    const float4x4 modelMat = surfaces[ input.objectId ].model;
    const float4x4 viewMat = view.viewMat;
    const float3 modelOrigin = float3( modelMat[0][3], modelMat[1][3], modelMat[2][3] );

	float4 albedoSample = float4( 1.0f, 1.0f, 1.0f, 1.0f );
	float3 normalSample = float3( 0.0f, 0.0f, 1.0f );
	float roughnessSample = material.roughness;
	float metalnessSample = material.metalness;
	
	float2 uv0 = input.uv0.xy;
	
	if ( isTextured && albedoTexId >= 0 )
	{
		Texture2D albedoTex = texSampler[ albedoTexId ];
		albedoSample = SrgbToLinear( albedoTex.Sample( bilinearSamplerWrap, uv0 ) );
	}

	if ( isTextured && normalTexId >= 0 )
	{
		Texture2D normalTex = texSampler[ normalTexId ];
		normalSample = DecodeNormal( normalTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
	}

	if ( isTextured && roughnessTexId >= 0 )
	{
		Texture2D roughnessTex = texSampler[ roughnessTexId ];
		roughnessSample = roughnessTex.Sample( bilinearSamplerWrap, uv0 ).r;
	}

	if ( isTextured && metalnessTexId >= 0 )
	{
		Texture2D metalnessTex = texSampler[ metalnessTexId ];
		metalnessSample = metalnessTex.Sample( bilinearSamplerWrap, uv0 ).r;
	}

	surfaceInput_t surfaceInput;

    const float normalBlendFactor = 1.0f;
	const float3 normal = lerp( float3( 0.0f, 0.0f, 1.0f ), normalize( normalSample.x * input.tangent + normalSample.y * input.bitangent + normalSample.z * input.TBN2), normalBlendFactor );

    const uint diffuseIBL = surfaces[ input.objectId ].diffuseIblCubeId;
    const uint specularIBL = surfaces[ input.objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;

    const int MaxReflectionLod = 4;

	const float ao = 1.0f;

	surfaceInput.N = normalize( normal );
	surfaceInput.V = normalize( view.viewOrigin.xyz - input.worldPosition.xyz );
	surfaceInput.NoV = saturate( dot( surfaceInput.N, surfaceInput.V ) );
	surfaceInput.cameraOrigin = view.viewOrigin.xyz;
	surfaceInput.positionWS = input.worldPosition.xyz;
	surfaceInput.albedo = albedoSample.rgb * diffuseColor;
	surfaceInput.roughness = saturate( globals.generic.x * roughnessSample + globals.generic.y );
	surfaceInput.metallic = saturate( globals.generic.z * metalnessSample + globals.generic.w );
	surfaceInput.F0 = lerp( float3( 0.04f, 0.04f, 0.04f ), surfaceInput.albedo.rgb, surfaceInput.metallic );
	
	surfaceInput.ccStrength = 1.0f;
	surfaceInput.ccRoughness = 0.0f;
	surfaceInput.ccNormal = normalize( normal );
	
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

#if 1
    for( int i = 0; i < (int)view.numLights; ++i )
    {
		const gpuLight_t light = lights[i];

		const lightingInput_t lightingInput = CalculateLightingInput( surfaceInput, light );

		float3 diffuse = ApplyLight( surfaceInput, light );

		//diffuse = ApplyClearcoat( surfaceInput, lightingInput, diffuse );
		
		const float shadowing = ApplyShadow( light.shadowViewId, surfaceInput.positionWS );

        Lo += shadowing * diffuse;
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

	float3 diffuse = AMBIENT * surfaceInput.albedo;
	if ( globals.useDiffuseIBL )
	{
		const float3 irradiance = cubeSamplers[diffuseIBL].Sample(bilinearSamplerWrap, CubeVector(surfaceInput.N)).rgb;
		diffuse = irradiance * surfaceInput.albedo;
	}
	const float3 ambient = ( kD * diffuse + specular ) * ao; // * material.Ka.rgb;

    float4 outColor;
    outColor.rgb = Lo + ambient;
    outColor.a = material.Tr;

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
