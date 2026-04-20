#ifndef LIGHT_HLSL_H
#define LIGHT_HLSL_H


// Surface sample data
struct surfaceInput_t
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
	float	ccStrength;	// cc: clear-coat
	float3	ccNormal;
	float	ccRoughness;
	float3	emissive;
	float	ao;
};


// Surface sample-to-light data
struct lightingInput_t
{
	float3	lightRay;
	float	lightDistance;
	float3	L;
	float3	H;

	float	NoL;
	float	NoH;
	float	LoH;
	float	HoV;
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

#endif // LIGHT_HLSL_H
