#ifndef BRDF_H
#define BRDF_H

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

float V_Kelemen( float LoH )
{
	return 0.25 / ( LoH * LoH );
}

#endif
