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
	float3	T;
	float3	B;
	float3	position;
	float3	albedo;
	float3	ccNormal;
	float3	emissive;
	float3	sheenColor;
	float3	tangentNormal;
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
	bool	useAniso;
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


const float2 PoissonDisk[ 16 ] =
{
	float2( -0.94201624, -0.39906216 ),
	float2( 0.94558609, -0.76890725 ),
	float2( -0.094184101, -0.92938870 ),
	float2( 0.34495938, 0.29387760 ),
	float2( -0.91588581, 0.45771432 ),
	float2( -0.81544232, -0.87912464 ),
	float2( -0.38277543, 0.27676845 ),
	float2( 0.97484398, 0.75648379 ),
	float2( 0.44323325, -0.97511554 ),
	float2( 0.53742981, -0.47373420 ),
	float2( -0.26496911, -0.41893023 ),
	float2( 0.79197514, 0.19090188 ),
	float2( -0.24188840, 0.99706507 ),
	float2( -0.81409955, 0.91437590 ),
	float2( 0.19984126, 0.78641367 ),
	float2( 0.14383161, -0.14100790 )
};


float ShadowPCF( Texture2D shadowMap, SamplerComparisonState samp,
	float3 shadowCoord, float radius, float2 seed )
{
	float angle = frac( sin( dot( seed, float2( 12.9898f, 78.233f ) ) ) * 43758.5453f ) * 6.28318f;
	float2x2 rot = float2x2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) );

	float texelSize = 1.0f / 2048.0f;
	float shadow = 0.0f;

	[unroll]
	for( int i = 0; i < 16; ++i )
	{
		float2 offset = mul( rot, PoissonDisk[ i ] ) * radius * texelSize;
		shadow += shadowMap.SampleCmpLevelZero( samp, shadowCoord.xy + offset, shadowCoord.z );
	}

	return shadow / 16.0f;
}


float4 SampleTexture( const gpuMaterial_t material, const int materialTextureSlot, const float2 uv[ 2 ] )
{
	const int textureUploadId = material.textureId[ materialTextureSlot ];
	const uint channel = min( MaxTextureUVs - 1, material.uvChannel[ materialTextureSlot ] );

	if( textureUploadId >= 0 )
	{
		const float2 transformedUv = mul( material.uvTransform[ materialTextureSlot ], uv[ channel ] ) + material.uvOffset[materialTextureSlot];

		return texSampler[ textureUploadId ].Sample( bilinearSamplerWrap, transformedUv );

	}
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}


float4 SampleTextureSrgb( const gpuMaterial_t material, const int materialTextureSlot, const float2 uv[ 2 ] )
{
	const int textureUploadId = material.textureId[ materialTextureSlot ];
	const uint channel = min( MaxTextureUVs - 1, material.uvChannel[ materialTextureSlot ] );

	if( textureUploadId >= 0 )
	{
		const float2 transformedUv = mul( material.uvTransform[ materialTextureSlot ], uv[ channel ] ) + material.uvOffset[ materialTextureSlot ];

		return SrgbToLinear( texSampler[ textureUploadId ].Sample( bilinearSamplerWrap, transformedUv.xy ) );
	}
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}


float3 SampleTextureNormal( const gpuMaterial_t material, const int materialTextureSlot, const float2 uv[ 2 ] )
{
	const int textureUploadId = material.textureId[ materialTextureSlot ];
	const uint channel = min( MaxTextureUVs - 1, material.uvChannel[ materialTextureSlot ] );

	if( textureUploadId >= 0 )
	{
		const float2 transformedUv = mul( material.uvTransform[ materialTextureSlot ], uv[ channel ] ) + material.uvOffset[ materialTextureSlot ];

		return DecodeNormal( texSampler[ textureUploadId ].Sample( bilinearSamplerWrap, transformedUv ).xyz );
	}
	return float3( 0.0f, 0.0f, 1.0f );
}


surfaceInput_t CalculateSurfaceInput( const gpuGlobals_t globals, const gpuView_t view, const gpuSurface_t surface, const gpuMaterial_t material, const PS_Input input )
{
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

	const float2 uv[ 2 ] = { input.uv0.xy, input.uv1.xy };

	const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );

	if( isTextured )
	{
		albedoSample *= SampleTextureSrgb( material, GGX_ALBEDO_MAP_SLOT, uv ).rgb;

		normalSample = SampleTextureNormal( material, GGX_NORMAL_MAP_SLOT, uv ).xyz;

		roughnessSample *= SampleTexture( material, GGX_ROUGHNESS_MAP_SLOT, uv ).g;

		metalnessSample *= SampleTexture( material, GGX_METALLIC_MAP_SLOT, uv ).b;

		aoSample *= SampleTexture( material, GGX_AO_MAP_SLOT, uv ).r;

		emissiveSample *= SampleTextureSrgb( material, GGX_EMISSIVE_MAP_SLOT, uv ).rgb;

		ccSample *= SampleTexture( material, GGX_CC_MAP_SLOT, uv ).r;

		ccRoughnessSample *= SampleTexture( material, GGX_CC_ROUGHNESS_MAP_SLOT, uv ).r;

		ccNormalSample *= SampleTextureNormal( material, GGX_CC_NML_MAP_SLOT, uv ).xyz;

		sheenSample *= SampleTextureSrgb( material, GGX_SHEEN_COLOR_MAP_SLOT, uv ).rgb;

		sheenRoughnessSample *= SampleTexture( material, GGX_SHEEN_ROUGHNESS_MAP_SLOT, uv ).r;

		anisotropySample *= SampleTexture( material, GGX_ANISOTROPY_MAP_SLOT, uv ).r;

		transmissionSample *= SampleTexture( material, GGX_TRANSMISSION_MAP_SLOT, uv ).r;
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
	surfaceInput.useAniso = ( surfaceInput.aniso != 0.0f );

	surfaceInput.N = normalize( normal );
	surfaceInput.V = normalize( view.viewOrigin.xyz - input.worldPosition.xyz );
	surfaceInput.NoV = saturate( dot( surfaceInput.N, surfaceInput.V ) );
	surfaceInput.F0 = lerp( float3( 0.04f, 0.04f, 0.04f ), surfaceInput.albedo.rgb, surfaceInput.metallic );
	surfaceInput.F = F_SchlickRoughness( surfaceInput.NoV, surfaceInput.F0, surfaceInput.roughness );
	surfaceInput.T = input.tangent;
	surfaceInput.B = input.bitangent;
	
	surfaceInput.position = input.worldPosition.xyz;
	surfaceInput.tangentNormal = normalSample;

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
