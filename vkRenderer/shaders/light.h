#ifndef LIGHT_HLSL_H
#define LIGHT_HLSL_H


struct lightInput_t
{
	float3	N;
	float3	V;
	float3	positionWS;
	float3	cameraOrigin;
	float	NoV;
	float3	albedo;
	float	roughness;
	float	metallic;
	float3	F0;
};

// https://learnopengl.com/PBR/IBL/Specular-IBL
float RadicalInverse_VdC( uint bits )
{
    bits = ( bits << 16u ) | ( bits >> 16u );
    bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
    bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
    bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
    bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
    return float( bits ) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley( uint i, uint N )
{
    return float2( float( i ) / float( N ), RadicalInverse_VdC( i ) );
}

float3 ImportanceSampleGGX( float2 Xi, float3 N, float roughness )
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt( ( 1.0 - Xi.y ) / ( 1.0 + ( a * a - 1.0 ) * Xi.y ) );
    float sinTheta = sqrt( 1.0 - cosTheta * cosTheta );

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos( phi ) * sinTheta;
    H.y = sin( phi ) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs( N.z ) < 0.999 ? float3( 0.0, 0.0, 1.0 ) : float3( 1.0, 0.0, 0.0 );
    float3 tangent = normalize( cross( up, N ) );
    float3 bitangent = cross( N, tangent );

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize( sampleVec );
}

float LuminanceFromRGB( const float3 rgb )
{
    return dot( rgb, float3( 0.30f, 0.59f, 0.11f ) );
}

float3 ApplyClearcoat( const float3 baseLayerColor, const float clearcoatRoughness )
{
	//// Coat specular
	//float  Dc = D_GGX( clearcoatRoughness, NoH );
	//float  Vc = V_Kelemen( LoH );
	//vec3   Fc = F_Schlick( 0.04, LoH );
	//float  Fc_scalar = clearcoat * Fc.x;

	//float coatLobe = clearcoat * Dc * Vc * Fc.x;

	//// Attenuate base then add coat on top
	//vec3 result = ( 1.0 - Fc_scalar ) * baseLayerColor + coatLobe;

	return float3( 0.0f, 0.0f, 0.0f );
}

#endif // LIGHT_HLSL_H
