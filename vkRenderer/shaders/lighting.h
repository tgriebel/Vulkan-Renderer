#ifndef LIGHT_HLSL_H
#define LIGHT_HLSL_H

#include "globals.h"

// Three structs here used to represent data respective of the lighting equation
// surfaceInput_t: Data from the current surface/pixel sample (one per shader invocation)
// lightingInput_t: Incoming light data to the surface sample (multiple / shader)
// brdfSample_t: Data from what happens when light interacts with the surface sample

// Surface sample data
struct surfaceInput_t
{
	float3	N;
	float3	V;
	float3	F;
	float3	F0;
	float3	position;
	float3	albedo;
	float3	ccNormal;
	float3	emissive;
	float3	sheenColor;
	float	NoV;
	float	roughness;
	float	metallic;
	float	ccStrength;	// cc: clear-coat
	float	ccRoughness;
	float	ao;
	float	sheenRoughness;
	float	aniso;
	float	anisoRotation;
	bool	useClearCoat;
	bool	useSheen;
};


// Surface sample-to-light data
struct lightingInput_t
{
	float3	lightRay;
	float3	intensity;
	float3	L;
	float3	H;
	float3	Li;
	float	lightDistance;
	float	NoL;
	float	NoH;
	float	LoH;
	float	HoV;
};


// BRDF surface sample
struct brdfSample_t
{
	float3 Fd;	// Diffuse
	float3 Fr;	// Specular
	float3 F;	// Fresnel
};


float4 SampleTexture( const gpuMaterial_t material, const int textureId, const float2 uv )
{
	if( textureId >= 0 )
	{
		const float2 transformedUv = uv;// mul( material.uvTransform[ textureId ], float4( uv.xy, 1.0f, 0.0f ) );

		return texSampler[ textureId ].Sample( bilinearSamplerWrap, transformedUv );

	}
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}


float4 SampleTextureSrgb( const gpuMaterial_t material, const int textureId, const float2 uv )
{
	if( textureId >= 0 )
	{
		const float2 transformedUv = uv;// mul( material.uvTransform[ textureId ], float4( uv.xy, 1.0f, 0.0f ) );

		return SrgbToLinear( texSampler[ textureId ].Sample( bilinearSamplerWrap, transformedUv.xy ) );
	}
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}


float3 SampleTextureNormal( const gpuMaterial_t material, const int textureId, const float2 uv )
{
	if( textureId >= 0 )
	{
		const float2 transformedUv = uv;// mul( material.uvTransform[ textureId ], float4( uv.xy, 1.0f, 0.0f ) );

		return DecodeNormal( texSampler[ textureId ].Sample( bilinearSamplerWrap, transformedUv ).xyz );
	}
	return float3( 0.0f, 0.0f, 1.0f );
}


surfaceInput_t CalculateSurfaceInput( const gpuGlobals_t globals, const gpuView_t view, const gpuSurface_t surface, const gpuMaterial_t material, const PS_Input input )
{
	const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );
	const int albedoTexId = material.textureId[ GGX_ALBEDO_MAP_SLOT ];
	const int normalTexId = material.textureId[ GGX_NORMAL_MAP_SLOT ];
	const int roughnessTexId = material.textureId[ GGX_ROUGHNESS_MAP_SLOT ];
	const int metalnessTexId = material.textureId[ GGX_METALLIC_MAP_SLOT ];
	const int aoTexId = material.textureId[ GGX_AO_MAP_SLOT ];
	const int emissiveTexId = material.textureId[ GGX_EMISSIVE_MAP_SLOT ];
	const int ccTexId = material.textureId[ GGX_CC_MAP_SLOT ];
	const int ccRoughnessTexId = material.textureId[ GGX_CC_ROUGHNESS_MAP_SLOT ];
	const int ccNormalTexId = material.textureId[ GGX_CC_NML_MAP_SLOT ];
	const int sheenColorTexId = material.textureId[ GGX_SHEEN_COLOR_MAP_SLOT ];
	const int sheenRoughnessTexId = material.textureId[ GGX_SHEEN_ROUGHNESS_MAP_SLOT ];
	const int anisotropyTexId = material.textureId[ GGX_ANISOTROPY_MAP_SLOT ];
	const int transmissionTexId = material.textureId[ GGX_TRANSMISSION_MAP_SLOT ];

	const float3 specularColor = material.Ks.rgb;
	const float specularPower = material.Ns;

	const float3 modelOrigin = float3( surface.model[ 0 ][ 3 ], surface.model[ 1 ][ 3 ], surface.model[ 2 ][ 3 ] );

	float3 albedoSample = material.albedo.rgb;
	float3 normalSample = float3( 0.0f, 0.0f, 1.0f );
	float roughnessSample = material.roughness;
	float metalnessSample = material.metalness;
	float3 emissiveSample = material.emissiveStrength * material.Ke;
	float aoSample = 1.0f;
	float ccSample = material.clearcoatWeight;
	float ccRoughnessSample = clamp( material.clearcoatRoughness, 0.089, 1.0 );
	float3 ccNormalSample = float3( 0.0f, 0.0f, 1.0f );
	float3 sheenSample = material.sheenColor;
	float sheenRoughnessSample = clamp( material.sheenRoughness, 0.07f, 1.0f );
	float anisotropySample = material.anisotropy;
	float transmissionSample = material.transmissionFactor;

	const float2 uv0 = input.uv0.xy;

	if( isTextured )
	{
		//albedoSample *= SampleTextureSrgb( material, albedoTexId, uv0 ).rgb;

		//normalSample = SampleTextureNormal( material, normalTexId, uv0 ).xyz;

		if( albedoTexId >= 0 )
		{
			Texture2D albedoTex = texSampler[ albedoTexId ];
			albedoSample *= SrgbToLinear( albedoTex.Sample( bilinearSamplerWrap, uv0 ) ).rgb;
		}

		if( normalTexId >= 0 )
		{
			Texture2D normalTex = texSampler[ normalTexId ];
			normalSample = DecodeNormal( normalTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
		}

		if( roughnessTexId >= 0 )
		{
			Texture2D roughnessTex = texSampler[ roughnessTexId ];
			roughnessSample *= roughnessTex.Sample( bilinearSamplerWrap, uv0 ).g;
		}

		if( metalnessTexId >= 0 )
		{
			Texture2D metalnessTex = texSampler[ metalnessTexId ];
			metalnessSample *= metalnessTex.Sample( bilinearSamplerWrap, uv0 ).b;
		}

		if( aoTexId >= 0 )
		{
			Texture2D aoTex = texSampler[ aoTexId ];
			aoSample *= aoTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if( emissiveTexId >= 0 )
		{
			Texture2D emissiveTex = texSampler[ emissiveTexId ];
			emissiveSample *= SrgbToLinear( emissiveTex.Sample( bilinearSamplerWrap, uv0 ) ).rgb;
		}

		if( ccTexId >= 0 )
		{
			Texture2D ccTex = texSampler[ ccTexId ];
			ccSample *= ccTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if( ccRoughnessTexId >= 0 )
		{
			Texture2D ccRoughnessTex = texSampler[ ccRoughnessTexId ];
			ccRoughnessSample *= ccRoughnessTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if( ccNormalTexId >= 0 )
		{
			Texture2D ccNormalTex = texSampler[ ccNormalTexId ];
			ccNormalSample = DecodeNormal( ccNormalTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
		}

		if( sheenColorTexId >= 0 )
		{
			Texture2D sheenTex = texSampler[ sheenColorTexId ];
			sheenSample = SrgbToLinear( sheenTex.Sample( bilinearSamplerWrap, uv0 ).rgb );
		}

		if( sheenRoughnessTexId >= 0 )
		{
			Texture2D sheenRoughnessTex = texSampler[ sheenRoughnessTexId ];
			sheenRoughnessSample = sheenRoughnessTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if( anisotropyTexId >= 0 )
		{
			Texture2D anisotropyTex = texSampler[ anisotropyTexId ];
			anisotropySample = anisotropyTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}

		if( transmissionTexId >= 0 )
		{
			Texture2D transmissionTex = texSampler[ transmissionTexId ];
			transmissionSample = transmissionTex.Sample( bilinearSamplerWrap, uv0 ).r;
		}
	}

	const float normalBlendFactor = 1.0f;
	const float3 normal = lerp( float3( 0.0f, 0.0f, 1.0f ), ComputeNormalWS( normalSample, input.tangent, input.bitangent, input.TBN2 ), normalBlendFactor );

	surfaceInput_t surfaceInput = (surfaceInput_t)0;

	surfaceInput.albedo = albedoSample;
	surfaceInput.roughness = saturate( globals.generic.x * roughnessSample + globals.generic.y );
	surfaceInput.metallic = saturate( globals.generic.z * metalnessSample + globals.generic.w );
	surfaceInput.emissive = emissiveSample;
	surfaceInput.ao = aoSample;
	surfaceInput.sheenColor = sheenSample;
	surfaceInput.sheenRoughness = sheenRoughnessSample;
	surfaceInput.ccStrength = ccSample;
	surfaceInput.ccRoughness = ccRoughnessSample;
	surfaceInput.ccNormal = normalize( ComputeNormalWS( ccNormalSample, input.tangent, input.bitangent, input.TBN2 ) );
	surfaceInput.useClearCoat = ( ccSample > 0.0f );
	surfaceInput.useSheen = any( sheenSample > 0.0f ) && ( sheenRoughnessSample > 0.0f );

	surfaceInput.N = normalize( normal );
	surfaceInput.V = normalize( view.viewOrigin.xyz - input.worldPosition.xyz );
	surfaceInput.NoV = saturate( dot( surfaceInput.N, surfaceInput.V ) );
	surfaceInput.F0 = lerp( float3( 0.04f, 0.04f, 0.04f ), surfaceInput.albedo.rgb, surfaceInput.metallic );
	surfaceInput.F = F_SchlickRoughness( surfaceInput.NoV, surfaceInput.F0, surfaceInput.roughness );
	surfaceInput.position = input.worldPosition.xyz;

	return surfaceInput;
}

lightingInput_t CalculateLightingInput( const surfaceInput_t surfaceInput, const gpuLight_t light )
{
	lightingInput_t lightingInput;

	lightingInput.lightRay = ( light.lightPos.xyz - surfaceInput.position );
	lightingInput.lightDistance = length( lightingInput.lightRay );
	lightingInput.L = lightingInput.lightRay / lightingInput.lightDistance;
	lightingInput.H = normalize( surfaceInput.V + lightingInput.L );
	lightingInput.intensity = light.intensity.rgb;

	lightingInput.NoL = max( dot( surfaceInput.N, lightingInput.L ), 0.0f );
	lightingInput.NoH = max( dot( surfaceInput.N, lightingInput.H ), 0.0f );
	lightingInput.LoH = max( dot( lightingInput.L, lightingInput.H ), 0.0f );
	lightingInput.HoV = max( dot( lightingInput.H, surfaceInput.V ), 0.0f );

	const float attenuation = 1.0f / ( lightingInput.lightDistance * lightingInput.lightDistance );
	const float spotFalloff = 1.0f;
	const float3 radiance = attenuation * spotFalloff * lightingInput.intensity;

	lightingInput.Li = radiance;

	return lightingInput;
}

#endif // LIGHT_HLSL_H
