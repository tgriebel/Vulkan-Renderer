#include "globals.h"
#include "lightUtil.h"
#include "util.h"

struct SpecularIblConstants
{
    float4x4 viewMat;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, SpecularIblConstants )

// https://learnopengl.com/PBR/IBL/Specular-IBL
psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    // GLSL mat4(...) fills columns; HLSL float4x4(...) fills rows.
    // Use transpose() to match GLSL column-major construction.
    float4x4 glslSpace = transpose( float4x4( 0.0f, 0.0f, 1.0f, 0.0f,
                                              -1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f ) );

	const float4x4 viewMat = transpose( mul( glslSpace, imageProcess.viewMat ) );
    const float3 viewForward = -normalize( viewMat[ 2 ] ).xyz;
    const float3 viewRight = normalize( viewMat[ 0 ] ).xyz;
    const float3 viewUp = normalize( viewMat[ 1 ] ).xyz;
    const float3 viewVector = normalize( viewForward + ( 2.0f * input.uv0.x - 1.0f ) * viewRight + ( 2.0f * input.uv0.y - 1.0f ) * viewUp );

    float3 up = float3( 0.0, 1.0, 0.0 );
    float3 right = normalize( cross( up, viewVector ) );
    up = normalize( cross( viewVector, right ) );

    const float lodBias = -10.0f;

#if 0
    float3 tangentSample = float3( sin( 0.0f ) * cos( 0.0f ), sin( 0.0f ) * sin( 0.0f ), cos( 0.0f ) );
    float3 sampleVec = normalize( tangentSample.x * right + tangentSample.y * up + tangentSample.z * viewVector );
    output.outColor = localCubemaps[ 0 ].SampleBias( bilinearSamplerClampEdge, sampleVec, lodBias );
    // output.outColor.rgb = 0.5f * ( sampleVec + float3( 1.0f, 1.0f, 1.0f ) );
    output.outColor.a = 1.0f;
#else
    float3 N = normalize( viewVector );
    float3 R = N;
    float3 V = R;

    const int mipLevels = GetTextureLevelsCube( localCubemaps[ 0 ] );
    const float roughness = level / float( mipLevels - 1 );

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3( 0.0, 0.0, 0.0 );
    for ( uint i = 0u; i < SAMPLE_COUNT; ++i )
    {
        float2 Xi = Hammersley( i, SAMPLE_COUNT );
        float3 H = ImportanceSampleGGX( Xi, N, roughness );
        float3 L = normalize( 2.0 * dot( V, H ) * H - V );

        float NdotL = max( dot( N, L ), 0.0 );
        if ( NdotL > 0.0 )
        {
			prefilteredColor += localCubemaps[ 0 ].SampleBias( bilinearSamplerClampEdge, L, lodBias).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    output.outColor = float4( prefilteredColor, 1.0 );
#endif

    return output;
}
