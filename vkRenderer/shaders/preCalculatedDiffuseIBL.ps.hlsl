#include "globals.h"
#include "util.h"

struct DiffuseIblConstants
{
    float4x4 viewMat;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, DiffuseIblConstants )

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

    float3 irradiance = float3( 0.0f, 0.0f, 0.0f );

    const float lodBias = -10.0f;

#if 0
    float3 tangentSample = float3( sin( 0.0f ) * cos( 0.0f ), sin( 0.0f ) * sin( 0.0f ), cos( 0.0f ) );
    float3 sampleVec = normalize( tangentSample.x * right + tangentSample.y * up + tangentSample.z * viewVector );
    output.outColor = localCubemaps[ 0 ].SampleBias( bilinearSamplerClampEdge, sampleVec, lodBias );
    //output.outColor.rgb = 0.5f * ( sampleVec + float3( 1.0f, 1.0f, 1.0f ) );
    output.outColor.a = 1.0f;
#else
    // https://learnopengl.com/PBR/IBL/Diffuse-irradiance
    float sampleDelta = 0.025f;
    float nrSamples = 0.0f;
    for ( float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta )
    {
        for ( float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta )
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3( sin( theta ) * cos( phi ), sin( theta ) * sin( phi ), cos( theta ) );
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * viewVector;
            sampleVec = normalize( sampleVec );

			irradiance += localCubemaps[ 0 ].SampleBias( bilinearSamplerClampEdge, sampleVec, lodBias).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * ( 1.0f / float( nrSamples ) );

    output.outColor = float4( irradiance, 1.0f );
#endif

    return output;
}
