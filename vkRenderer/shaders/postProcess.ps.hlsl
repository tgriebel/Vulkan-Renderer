#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )


float SampleCoverage( const int2 pos, Texture2D depthTexture )
{
    return depthTexture.Load( int3( pos, 0 ) ).g;
}


float EdgeOutline( const int2 pixelLocation, Texture2D depthTexture )
{
    const float c00 = SampleCoverage( pixelLocation + int2( -1, -1 ), depthTexture );
    const float c10 = SampleCoverage( pixelLocation + int2( 0, -1 ), depthTexture );
    const float c20 = SampleCoverage( pixelLocation + int2( 1, -1 ), depthTexture );
    const float c01 = SampleCoverage( pixelLocation + int2( -1, 0 ), depthTexture );
    const float c11 = SampleCoverage( pixelLocation + int2( 0, 0 ), depthTexture );
    const float c21 = SampleCoverage( pixelLocation + int2( 1, 0 ), depthTexture );
    const float c02 = SampleCoverage( pixelLocation + int2( -1, 1 ), depthTexture );
    const float c12 = SampleCoverage( pixelLocation + int2( 0, 1 ), depthTexture );
    const float c22 = SampleCoverage( pixelLocation + int2( 1, 1 ), depthTexture );

    const float minCoverage = min( min( min( c00, c10 ), min( c20, c01 ) ),
        min( min( c11, c21 ), min( c02, min( c12, c22 ) ) ) );
    const float maxCoverage = max( max( max( c00, c10 ), max( c20, c01 ) ),
        max( max( c11, c21 ), max( c02, max( c12, c22 ) ) ) );

    const float edge = maxCoverage - minCoverage;

    const float outerStrength = 0.1f;
    const float innerStrength = 0.8f;
    const float stencilCoverage = smoothstep( outerStrength, innerStrength, edge );

    return stencilCoverage;
}


float3 ApplyBloom( const Texture2D bloomTexture, const float3 sceneColor, const float2 uv )
{
	float3 bloomColor;
	
	if ( globals.bloom.x > 0.0f )
	{
		const float3 bloom = bloomTexture.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).rgb;
		const float3 bloomHdr = lerp( sceneColor, bloom, globals.bloom.y );

		bloomColor = bloomHdr;
	}
	else
	{
		bloomColor = sceneColor;
	}
	return bloomColor;
}


float3 ApplyTonemap( const Texture2D luminanceTexture, const float3 sceneColor )
{
	float3 tonemapColor = sceneColor;
	
	const float middleGrey = globals.exposure.x;
	const float reinhardAlpha = clamp( middleGrey, 0.045f, 0.72f ); // Suggested middle-grey range from reinhard paper

	if ( globals.exposure2.x == 1.0f )
	{
		const float maxLod = float( GetTextureLevels( luminanceTexture ) - 1 );
		const float luminance = luminanceTexture.SampleLevel( bilinearSamplerClampEdge, float2( 0.5f, 0.5f ), maxLod ).r;

        const float exposure = reinhardAlpha / clamp( luminance, 0.005f, 10000.0f );

		tonemapColor *= exposure;
	}
	return tonemapColor;
}


float3 ApplyChromaticAberration( const Texture2D sceneTexture, const float3 sceneColor, const float2 uv )
{
	float3 caColor;
	
	if ( globals.chromaticAberration.x != 0.0f )
	{
		const float intensity = globals.chromaticAberration.y;

		const float2 dir = ( uv - 0.5f ); // direction from center
		const float dist = length( dir );

		const float2 offset = dir * dist * intensity;

		const float r = sceneTexture.Sample( bilinearSamplerClampEdge, uv + offset ).r;
		const float g = sceneColor.g;
		const float b = sceneTexture.Sample( bilinearSamplerClampEdge, uv - offset ).b;
		
		caColor = float3( r, g, b );
	}
	else
	{
		caColor = sceneColor;
	}
	return caColor;
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];
	const gpuMaterial_t material = materials[materialId];

    const uint textureId0 = material.textureId[ 0 ];
	const uint textureId1 = material.textureId[ 1 ];
	const uint textureId2 = material.textureId[ 2 ];
	const uint textureId3 = material.textureId[ 3 ];
	const uint textureId4 = material.textureId[ 4 ];
	const uint textureId5 = material.textureId[ 5 ];
	
	Texture2D sceneTexture = localTextures[ textureId0 ];
	Texture2D depthTexture = localTextures[ textureId1 ];
	Texture2D dofTexture = localTextures[ textureId2 ];
    Texture2D luminanceTexture = localTextures[ textureId3 ];
	Texture2D bloomTexture = localTextures[ textureId4 ];
	Texture2D dofCocTexture = localTextures[ textureId5 ];

    const float4x4 viewMat = view.viewMat;
    const float3 forward = -normalize( viewMat[2].xyz );
    const float3 up = normalize( viewMat[0].xyz );
    const float3 right = normalize( viewMat[1].xyz );
    const float3 viewVector = normalize( forward + input.uv0.x * up + input.uv0.y * right );

    const int2 pixelLocation = int2( view.dimensions.xy * input.uv0.xy );

    const float stencilCoverage = EdgeOutline( pixelLocation, depthTexture );

    float4 sceneColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );

    const float4 uvColor = float4( input.uv0.xy, 0.0f, 1.0f );

    output.outColor.rgb = float3( 0.0f, 0.0f, 0.0f );
    const bool dofEnabled = ( globals.dof.x != 0.0f );
    const float coc = dofCocTexture.Sample( bilinearSamplerClampEdge, input.uv0.xy ).r;

    float3 hdrColor;
    if ( dofEnabled && ( abs( coc ) > 0.0f ) ){
        hdrColor.rgb = dofTexture.Sample( bilinearSamplerClampEdge, input.uv0.xy ).rgb;
    } else {
        hdrColor.rgb = sceneTexture.Sample( bilinearSamplerClampEdge, input.uv0.xy ).rgb;
    }
	
    const float3 tint = globals.toneMapTint.rgb;

	float3 finalColor = tint * hdrColor;

	finalColor = ApplyChromaticAberration( sceneTexture, finalColor, input.uv0.xy );

	finalColor = ApplyBloom( bloomTexture, finalColor, input.uv0.xy );

	finalColor = ApplyTonemap( luminanceTexture, finalColor );

	sceneColor.rgb = LinearToSrgb( finalColor );

    output.outColor.rgb = lerp( sceneColor.rgb, float3( 0.0f, 1.0f, 0.0f ), stencilCoverage );
    output.outColor.a = 1.0f;

    return output;
}
