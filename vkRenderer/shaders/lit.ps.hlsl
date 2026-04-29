#include "globals.h"
#include "color.h"
#include "util.h"
#include "brdf.h"

PS_LAYOUT_STANDARD( Texture2D ) // Must come before lighting.h

#include "lighting.h"

// Evaluate BRDF functions evaluate a particular BRDF at a surface sample
// Apply* functions modify some BRDF
// Clearcoat attenuates the base BRDF, Sheen simply adds on top

brdfSample_t EvaluateBaseBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput )
{
	const float perceptualRoughness = surfaceInput.roughness;
	const float metallic = surfaceInput.metallic;
	const float3 F0 = surfaceInput.F0;

	const float Dc = D_GGX( lightingInput.NoH, perceptualRoughness );
	const float Gc = G_Smith( surfaceInput.NoV, lightingInput.NoL, perceptualRoughness);
	const float3 Fc = F_Schlick( lightingInput.HoV, F0 );

	const float3 kS = Fc;
	float3 kD = float3( 1.0f, 1.0f, 1.0f ) - kS;
	kD *= 1.0f - metallic;

	float3 numerator = Dc * Gc * Fc;
	float denominator = 4.0f * surfaceInput.NoV * lightingInput.NoL + 0.00001f;
	
    brdfSample_t brdf;
	
    brdf.Fr = numerator / denominator;
    brdf.Fd = ( kD * surfaceInput.albedo ) / PI;
    brdf.F = Fc;

	return brdf;
}

// FIXME: temp BS
brdfSample_t EvaluateAnisoBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput )
{
    const float perceptualRoughness = surfaceInput.roughness;
    const float metallic = surfaceInput.metallic;
    const float3 F0 = surfaceInput.F0;
    
    float sinRot, cosRot;
    sincos( surfaceInput.anisoRotation, sinRot, cosRot );
    const float3 T = cosRot * surfaceInput.T + sinRot * surfaceInput.B;
    const float3 B = -sinRot * surfaceInput.T + cosRot * surfaceInput.B;
    
    // Roughness split along tangent (at) and bitangent (ab)
    const float2 anisoR = AnisoRoughness( perceptualRoughness, surfaceInput.aniso );
    const float at = anisoR.x;
    const float ab = anisoR.y;
    
    // Project V and L onto the anisotropic tangent frame
    const float ToV = dot( T, surfaceInput.V );
    const float BoV = dot( B, surfaceInput.V );
    const float ToL = dot( T, lightingInput.L );
    const float BoL = dot( B, lightingInput.L );

    const float Dc = D_GGX_Aniso( lightingInput.NoH, lightingInput.H, T, B, at, ab );
    const float Gc = V_SmithGGXCorrelated_Aniso( at, ab, ToV, BoV, ToL, BoL, surfaceInput.NoV, lightingInput.NoL );
    const float3 Fc = F_Schlick( lightingInput.HoV, F0 );

    float3 kD = ( float3( 1.0f, 1.0f, 1.0f ) - Fc ) * ( 1.0f - metallic );

    brdfSample_t brdf;
    brdf.Fr = Dc * Gc * Fc;
    brdf.Fd = ( kD * surfaceInput.albedo ) / PI;
    brdf.F = Fc;

    return brdf;
}


float3 ApplyShadow( const uint shadowViewId, float3 worldPosition, const float3 Lo )
{
	float shadowing = 1.0f; // Assumes spot-light, should be 0.0f for normal lights
	
	if ( shadowViewId != 0xFF )
	{
		const gpuView_t shadowView = views[shadowViewId];
		
		const uint shadowMapTexId = shadowViewId;
		
		Texture2D shadowMap = localTextures[ shadowMapTexId ];

		const float shadowBias = 0.001f;
	
        // Light Space Position
		float4 lsPosition = mul( mul( shadowView.projMat, shadowView.viewMat ), float4( worldPosition.xyz, 1.0f ) );
		lsPosition.xyz /= lsPosition.w;

		lsPosition.z -= shadowBias;
		
		const float2 ndc = 0.5f * lsPosition.xy + 0.5f;

		const float spotRadius = 0.3f;
		const bool withinSpotlight = ( length(ndc.xy - float2( 0.5f, 0.5f ) ) < spotRadius ); // Similar to an SDF. Distance from center below a threshold
		
		if ( withinSpotlight )
		{
            const float shadowMapSample = shadowMap.SampleCmpLevelZero( depthShadowSampler, ndc.xy, lsPosition.z );

			shadowing = shadowMapSample * globals.shadowParms.w;
		}
		else
		{
			shadowing = 0.0f;
		}
	}
    return ( shadowing * Lo );
}


void ApplyClearcoatBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput, inout brdfSample_t brdf )
{
    const float NoH = saturate( dot( surfaceInput.ccNormal, lightingInput.H ) );
    const float NoL = saturate( dot( surfaceInput.ccNormal, lightingInput.L ) );

	const float F0 = 0.04f;
	
    const float Dc = D_GGX( NoH, surfaceInput.ccRoughness );
    const float Vc = V_Kelemen( lightingInput.LoH );
    const float Fc = surfaceInput.ccStrength * F_Schlick( lightingInput.LoH, F0 ).x;
	
    float clearcoat = ( Dc * Vc ) * Fc;

	// Energy loss from base: attenuate by Fresnel of clearcoat
    const float attenuation = ( 1.0f - Fc );
	
    brdf.Fd *= attenuation;
    brdf.Fr *= attenuation * attenuation;
}


void ApplySheenBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput, inout brdfSample_t brdf )
{
    const float Dc = D_Charlie( lightingInput.NoH, surfaceInput.sheenRoughness );
    const float Vc = V_Neubelt( surfaceInput.NoV, lightingInput.NoL );

    const float3 sheenLobe = surfaceInput.sheenColor * Dc * Vc;
    
    const float sheenMax = max( surfaceInput.sheenColor.r, max( surfaceInput.sheenColor.g, surfaceInput.sheenColor.b ) );
    const float sheenScaling = 1.0f - sheenMax * 0.157f; // TODO: replace magic number with another BRDF LUT

    // Need to attenuate base energy
    brdf.Fd *= sheenScaling;
    brdf.Fr *= sheenScaling;
    brdf.Fr += sheenLobe;
}


float3 EvaluateDiffuseAmbient( TextureCube diffuseIBL, const surfaceInput_t surfaceInput )
{
    float3 kS = surfaceInput.F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - surfaceInput.metallic;

    float3 ambientHemisphere = AMBIENT.rgb * surfaceInput.albedo; // Overriden by IBL if enabled
    if ( globals.useDiffuseIBL )
    {
        const float3 irradiance = diffuseIBL.Sample( bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
        ambientHemisphere = irradiance * surfaceInput.albedo;
        
        // Based on CC IBL note in: https://google.github.io/filament/Filament.md.html
        if ( surfaceInput.useClearCoat )
        {
            const float3 clearcoatF0 = float3( 0.04f, 0.04f, 0.04f );
            const float ccNoV = saturate( dot( surfaceInput.ccNormal, surfaceInput.V ) );
            float3 clearCoatF = surfaceInput.ccStrength * F_SchlickRoughness( ccNoV, clearcoatF0, surfaceInput.ccRoughness );
            kD *= ( 1.0f - clearCoatF );
        }
        
        if ( surfaceInput.useSheen )
        {
            const float sheenMax = max( surfaceInput.sheenColor.r, max( surfaceInput.sheenColor.g, surfaceInput.sheenColor.b ) );
            const float sheenScaling = 1.0f - sheenMax * 0.157f; // TODO: replace magic number with another BRDF LUT
            kD *= sheenScaling;

            const float3 sheenIrradiance = diffuseIBL.Sample( bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
            kD += surfaceInput.sheenColor * sheenIrradiance;
        }
    }
    kD *= ambientHemisphere;
	
    return kD;
}


float3 EvaluateSpecularAmbient( TextureCube specularIBL, Texture2D brdfLUT, const surfaceInput_t surfaceInput )
{
    float3 specular = float3( 0.0f, 0.0f, 0.0f );
    if ( globals.useSpecularIBL )
    {
        const float3 R = reflect( -surfaceInput.V, surfaceInput.N );
        const int MipLevels = (int)GetTextureLevelsCube( specularIBL ) - 1;
        const float3 specIBL = specularIBL.SampleLevel( bilinearSamplerWrap, CubeVector( R ), surfaceInput.roughness * MipLevels ).rgb;

        const float2 envBRDF = brdfLUT.Sample( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ) ).xy;
        specular = specIBL * ( surfaceInput.F * envBRDF.x + envBRDF.y );
    
        // Based on CC IBL note in: https://google.github.io/filament/Filament.md.html
        if ( surfaceInput.useClearCoat )
        {
            const float ccNoV = saturate( dot( surfaceInput.ccNormal, surfaceInput.V ) );
        
            const float3 ccR = reflect( -surfaceInput.V, surfaceInput.ccNormal );          
            const float3 ccSpecIBL = specularIBL.SampleLevel( bilinearSamplerWrap, CubeVector( ccR ), surfaceInput.ccRoughness * MipLevels ).rgb;
    
            const float3 clearcoatF0 = float3( 0.04f, 0.04f, 0.04f );
            float3 clearCoatF = surfaceInput.ccStrength * F_SchlickRoughness( ccNoV, clearcoatF0, surfaceInput.ccRoughness );

            specular *= ( 1.0f - clearCoatF ) * ( 1.0f - clearCoatF );
            specular += clearCoatF * ccSpecIBL;
        }
    } 
    return specular;
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[ viewlId ];
	const gpuMaterial_t material = materials[ materialId ];
    const gpuSurface_t surface = surfaces[ input.objectId ];

    const uint diffuseIBL = surfaces[ input.objectId ].diffuseIblCubeId;
    const uint specularIBL = surfaces[ input.objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;
	
    const surfaceInput_t surfaceInput = CalculateSurfaceInput( globals, view, surface, material, input );
	
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

#if 1
    for( int i = 0; i < (int)view.numLights; ++i )
    {
		const gpuLight_t light = lights[i];

		const lightingInput_t lightingInput = CalculateLightingInput( surfaceInput, light );

        brdfSample_t brdf;
        
        if ( surfaceInput.useAniso ) {
            brdf = EvaluateAnisoBrdf( surfaceInput, lightingInput );
        } else {
            brdf = EvaluateBaseBrdf( surfaceInput, lightingInput );
        }
	
        if ( surfaceInput.useClearCoat ) {
			ApplyClearcoatBrdf( surfaceInput, lightingInput, brdf );
        }
        if ( surfaceInput.useSheen ) {
            ApplySheenBrdf( surfaceInput, lightingInput, brdf );
        }
		
        float3 Lo_i = ( brdf.Fd + brdf.Fr ) * lightingInput.Li * lightingInput.NoL;
		
		Lo_i = ApplyShadow( light.shadowViewId, surfaceInput.position, Lo_i );

        Lo += Lo_i;
    }
#endif

    const float3 kD = EvaluateDiffuseAmbient( globalCubemaps[ diffuseIBL ], surfaceInput );
    const float3 specularAmbient = EvaluateSpecularAmbient( globalCubemaps[ specularIBL ], globalTextures[ brdfLutId ], surfaceInput );

    const float3 ambient = ( kD + specularAmbient ) * surfaceInput.ao;

    float4 outColor;
	outColor.rgb = Lo + ambient + surfaceInput.emissive;
    outColor.a = material.opacity;

#define DEBUG_LIGHTING
    
#ifdef DEBUG_LIGHTING
    
    switch ( globals.debugLightingMode )
    {
        case DEBUG_ALBEDO:
            outColor.rgb = surfaceInput.albedo;
            break;
        
        case DEBUG_ROUGHNESS:
            outColor.rgb = surfaceInput.roughness;
            break;
        
        case DEBUG_METALLIC:
            outColor.rgb = surfaceInput.metallic;
            break;
        
        case DEBUG_TBN_NORMAL:
            outColor.rgb = 0.5f * surfaceInput.tangentNormal + float3( 0.5f, 0.5f, 0.5f );
            break;
        
        case DEBUG_NORMAL:
            outColor.rgb = 0.5f * surfaceInput.N + float3( 0.5f, 0.5f, 0.5f );
            break;
        
        case DEBUG_INPUT_UV:
            outColor.rgb = float3( input.uv0.xy, 0.0f );
            break;
        
        case DEBUG_EMISSIVE:
            outColor.rgb = surfaceInput.emissive;
            break;
        
        case DEBUG_SHEENCOLOR:
            outColor.rgb = surfaceInput.sheenColor;
            break;
        
        case DEBUG_SHEENROUGHNESS:
            outColor.rgb = surfaceInput.sheenRoughness;
            break;
        
        case DEBUG_AO:
            outColor.rgb = surfaceInput.ao.rrr;
            break;
        
                
        case DEBUG_BRDF_LUT:
            outColor.rg = globalTextures[ brdfLutId ].Sample( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ) ).rg;
            outColor.b = 0.0f;
            break;
        
        default:
            break;
    }
#endif
    
    psOutput_t output = (psOutput_t)0;

#ifdef USE_MRT
    float4 outColor1;
    outColor1.rgb = 0.5f * ( surfaceInput.N + float3( 1.0f, 1.0f, 1.0f ) );
    //outColor1.rgb = float3( input.uv0.xy, 0.0f );
    outColor1.a = 1.0f;

    output.outColor = ClampColorFp16( outColor );
    output.outColor1 = ClampColorFp16( outColor1 );
#else
    output.outColor = ClampColorFp16( outColor );
#endif
    return output;
}
