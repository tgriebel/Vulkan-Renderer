#ifndef LIGHT_HLSL_H
#define LIGHT_HLSL_H

float D_GGX( const float NoH, const float roughness )
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;

    float num = a2;
    float denom = ( NoH2 * ( a2 - 1.0 ) + 1.0 );
    denom = PI * denom * denom;

    return num / denom;
}

float G_SchlickGGX( const float cosAngle, const float roughness )
{
    const float r = ( roughness + 1.0 );
    const float k = ( r * r ) / 8.0f;

    const float num = cosAngle;
    const float denom = cosAngle * ( 1.0f - k ) + k;

    return ( num / denom );
}

float G_Smith( const float NoV, const float NoL, const float roughness )
{
    const float ggx2 = G_SchlickGGX( NoV, roughness );
    const float ggx1 = G_SchlickGGX( NoL, roughness );
    return ( ggx1 * ggx2 );
}

float3 F_Schlick( float cosTheta, float3 F0 )
{
    return F0 + ( 1.0f - F0 ) * pow( clamp( 1.0f - cosTheta, 0.0f, 1.0f ), 5.0f );
}

float3 F_SchlickRoughness( float cosTheta, float3 F0, float roughness )
{
    return F0 + ( max( float3( 1.0f - roughness, 1.0f - roughness, 1.0f - roughness ), F0 ) - F0 ) * pow( clamp( 1.0f - cosTheta, 0.0f, 1.0f ), 5.0f );
}

float Fd_Lambert()
{
    return 1.0 / PI;
}

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

#endif // LIGHT_HLSL_H
