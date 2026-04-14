#include "globals_hlsl.h"
#include "color_hlsl.h"
#include "light_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )


float SampleCoverage( const int2 pos, const uint stencilTextureId ) {
    return codeSamplers[ stencilTextureId ].Load( int3( pos, 0 ) ).g;
}


float EdgeOutline( const int2 pixelLocation, const uint stencilTextureId )
{
    const float c00 = SampleCoverage( pixelLocation + int2( -1, -1 ), stencilTextureId );
    const float c10 = SampleCoverage( pixelLocation + int2( 0, -1 ), stencilTextureId );
    const float c20 = SampleCoverage( pixelLocation + int2( 1, -1 ), stencilTextureId );
    const float c01 = SampleCoverage( pixelLocation + int2( -1, 0 ), stencilTextureId );
    const float c11 = SampleCoverage( pixelLocation + int2( 0, 0 ), stencilTextureId );
    const float c21 = SampleCoverage( pixelLocation + int2( 1, 0 ), stencilTextureId );
    const float c02 = SampleCoverage( pixelLocation + int2( -1, 1 ), stencilTextureId );
    const float c12 = SampleCoverage( pixelLocation + int2( 0, 1 ), stencilTextureId );
    const float c22 = SampleCoverage( pixelLocation + int2( 1, 1 ), stencilTextureId );

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


PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const view_t view = views[ viewlId ];
    const material_t material = materials[ materialId ];

    const uint textureId0 = material.textureId0;
    const uint textureId1 = material.textureId1;
    const uint textureId2 = material.textureId2;
    const uint textureId3 = material.textureId3;
    const uint textureId4 = material.textureId4;

    const float4x4 viewMat = view.viewMat;
    const float3 forward = -normalize( viewMat[2].xyz );
    const float3 up = normalize( viewMat[0].xyz );
    const float3 right = normalize( viewMat[1].xyz );
    const float3 viewVector = normalize( forward + input.uv0.x * up + input.uv0.y * right );

    const int2 pixelLocation = int2( view.dimensions.xy * input.uv0.xy );

    const float stencilCoverage = EdgeOutline( pixelLocation, textureId1 );

    float4 sceneColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );

    const float4 uvColor = float4( input.uv0.xy, 0.0f, 1.0f );
    const float sceneDepth = codeSamplers[ textureId1 ].Load( int3( pixelLocation, 0 ) ).r;
    const float skyMask = ( sceneDepth > 0.0f ) ? 1.0f : 0.0f;
    const float4 skyColor = float4( cubeSamplers[ textureId0 ].Sample( cubeSamplersSt, float3( -viewVector.y, viewVector.z, viewVector.x ) ).rgb, 1.0f );

    output.outColor.rgb = float3( 0.0f, 0.0f, 0.0f );
    const bool enabled = globals.dof.x != 0.0f;
    const float focalDepth = globals.dof.y;
    const float focalRange = globals.dof.z;
    float coc = ( sceneDepth - focalDepth ) / focalRange;
    const int MAX_MIP_LEVELS = 3;
    coc = clamp( coc, -1.0, 1.0f );

    float3 hdrColor;
    if ( enabled && ( coc < 0.0f ) ) {
        hdrColor.rgb = codeSamplers[ textureId2 ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy, int( -coc * MAX_MIP_LEVELS ) ).rgb;
    } else {
        hdrColor.rgb = codeSamplers[ textureId0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy ).rgb;
    }

    const float3 tint = globals.toneMapTint.rgb;

    float3 exposureAdjustedColor = tint;

    if ( globals.bloom.x > 0.0f )
    {
        const float3 bloom = codeSamplers[ textureId4 ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy, 0 ).rgb;
        const float3 bloomHdr = lerp( hdrColor, bloom, globals.bloom.y );

        exposureAdjustedColor *= bloomHdr;
    }
    else
    {
        exposureAdjustedColor *= hdrColor;
    }

    const float middleGrey = globals.exposure.y;
    const float reinhardAlpha = clamp( middleGrey, 0.045f, 0.72f ); // Suggested middle-grey range from reinhard paper

    if( globals.exposure2.x == 1.0f )
    {
        const float maxLod = float( GetTextureLevels( codeSamplers[ textureId3 ] ) - 1 );
        const float luminance = codeSamplers[ textureId3  ].SampleLevel( codeSamplersSt, float2( 0.5f, 0.5f ), maxLod ).r;

        const float exposure = reinhardAlpha / clamp( luminance, 0.005f, 10000.0f );

        exposureAdjustedColor *= exposure;
    }

    sceneColor.rgb = LinearToSrgb( exposureAdjustedColor );

    output.outColor.rgb = lerp( sceneColor.rgb, float3( 0.0f, 1.0f, 0.0f ), stencilCoverage );
    output.outColor.a = 1.0f;

    return output;
}
