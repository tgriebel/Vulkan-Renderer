#include "globals.h"
#include "color.h"
#include "util.h"
#include "brdf.h"

PS_LAYOUT_STANDARD( Texture2D ) // Must come before lighting.h

#include "lighting.h"

#ifdef USE_MRT
PS_LAYOUT_MRT_1_OUT
#endif

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
	float denominator = 4.0f * surfaceInput.NoV * lightingInput.NoL + 0.0001f;
	
    brdfSample_t brdf;
	
    brdf.Fr = numerator / denominator;
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
    const float NoV = surfaceInput.NoV;
    const float NoH = lightingInput.NoH;
    const float NoL = lightingInput.NoL;
	
    const float Dc = D_Charlie( NoH, surfaceInput.sheenRoughness );
    const float Vc = V_Neubelt( NoV, NoL );
	
    brdf.F += surfaceInput.sheenColor;
    brdf.Fr += ( Dc * Vc ) * brdf.F;
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
    }
    kD *= ambientHemisphere;
	
    return kD;
}


float3 EvaluateSpecularAmbient( TextureCube specularIBL, Texture2D brdfLUT, const surfaceInput_t surfaceInput )
{
    const int MaxReflectionLod = 16;
	
    float3 specular = float3( 0.0f, 0.0f, 0.0f );
    if ( globals.useSpecularIBL )
    {
        const float3 R = reflect( -surfaceInput.V, surfaceInput.N );
        const int MipLevels = min( (int)GetTextureLevelsCube( specularIBL ), MaxReflectionLod );
        const float3 specIBL = specularIBL.SampleLevel( bilinearSamplerWrap, CubeVector( R ), surfaceInput.roughness * MipLevels ).rgb;

        const float2 envBRDF = brdfLUT.Sample( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ) ).rg;
        specular = specIBL * ( surfaceInput.F * envBRDF.x + envBRDF.y );
    }
    return specular;
}


#ifdef USE_MRT
PS_Output_MRT PSMain( PS_Input input )
#else
PS_Output PSMain( PS_Input input )
#endif
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

        brdfSample_t brdf = EvaluateBaseBrdf( surfaceInput, lightingInput );
			
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

    const float3 specularAmbient = EvaluateSpecularAmbient( cubeSamplers[ specularIBL ], texSampler[ brdfLutId ], surfaceInput );
    const float3 kD = EvaluateDiffuseAmbient( cubeSamplers[ diffuseIBL ], surfaceInput );
	
	float3 ccSpecularAmbient = float3( 0.0f, 0.0f, 0.0f );
	//if ( surfaceInput.useClearCoat )
	//{
	//	const float Fc_ibl = ( 0.04f + 0.96f * pow( clamp( 1.0f - surfaceInput.NoV, 0.0f, 1.0f ), 5.0f ) ) * surfaceInput.ccStrength;

	//	//if ( globals.useSpecularIBL )
	//	{
	//		const float3 R_cc        = reflect( -surfaceInput.V, surfaceInput.ccNormal );
	//		const int    ccMipLevels = min( (int)GetTextureLevelsCube( cubeSamplers[ specularIBL ] ), MaxReflectionLod );
	//		const float3 ccIBL       = cubeSamplers[ specularIBL ].SampleLevel( bilinearSamplerWrap, CubeVector( R_cc ), surfaceInput.ccRoughness * ccMipLevels ).rgb;
	//		ccSpecularAmbient        = ccIBL * Fc_ibl;
	//	}

	//	// Attenuate base layers — same energy conservation as direct lighting
	//	diffuse  *= ( 1.0f - Fc_ibl );
	//	specular *= ( 1.0f - Fc_ibl ) * ( 1.0f - Fc_ibl );
	//}

    const float3 ambient = ( kD + specularAmbient + ccSpecularAmbient ) * surfaceInput.ao;

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
