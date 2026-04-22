#ifndef BRDF_H
#define BRDF_H

// General references
// https ://google.github.io/filament/Filament.md.html
// https://learnopengl.com/PBR/Theory

// ============================================================
// GGX
// ============================================================

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

// ============================================================
// Clearcoat
// ============================================================

float V_Kelemen( float LoH )
{
	return 0.25 / ( LoH * LoH );
}

// ============================================================
// Blinn Phong
// ============================================================

float D_BlinnPhong( const float NoH, const float shininess )
{
	return ( ( shininess + 2.0f ) / ( 2.0f * PI ) ) * pow( NoH, shininess );
}

float3 BlinnPhong( const float3 Kd, const float3 Ks, const float Ns,
				   const float NoH, const float NoL )
{
	float3 diffuse = Kd * Fd_Lambert();
	float3 specular = Ks * D_BlinnPhong( NoH, Ns );

	return ( diffuse + specular ) * NoL;
}

// ============================================================
// Anisotropic GGX
// ============================================================

float2 AnisoRoughness( const float roughness, const float anisotropy )
{
	return float2(
		max( roughness * ( 1.0f + anisotropy ), 0.001f ),
		max( roughness * ( 1.0f - anisotropy ), 0.001f )
	);
}

float D_GGX_Aniso( const float NoH, const float ToH, const float BoH,
				   const float alphaT, const float alphaB )
{
	float a = ToH / alphaT;
	float b = BoH / alphaB;
	float c = a * a + b * b + NoH * NoH;
	return 1.0f / ( PI * alphaT * alphaB * c * c );
}

float V_SmithGGX_Aniso( const float NoV, const float NoL,
						 const float ToV, const float BoV,
						 const float ToL, const float BoL,
						 const float alphaT, const float alphaB )
{
	float lambdaV = NoL * length( float3( alphaT * ToV, alphaB * BoV, NoV ) );
	float lambdaL = NoV * length( float3( alphaT * ToL, alphaB * BoL, NoL ) );
	return 0.5f / ( lambdaV + lambdaL );
}

// ============================================================
// Sheen
// ============================================================

float D_Ashikhmin( float roughness, float NoH )
{
	// Ashikhmin 2007, "Distribution-based BRDFs"
	float a2 = roughness * roughness;
	float cos2h = NoH * NoH;
	float sin2h = max( 1.0 - cos2h, 0.0078125 ); // 2^(-14/2), so sin2h^2 > 0 in fp16
	float sin4h = sin2h * sin2h;
	float cot2 = -cos2h / ( a2 * sin2h );
	return 1.0 / ( PI * ( 4.0 * a2 + 1.0 ) * sin4h ) * ( 4.0 * exp( cot2 ) + sin4h );
}

float D_Charlie( const float NoH, const float roughness )
{
	float invAlpha = 1.0f / roughness;
	float sinH = sqrt( 1.0f - NoH * NoH );
	return ( 2.0f + invAlpha ) * pow( sinH, invAlpha ) / ( 2.0f * PI );
}

float V_Neubelt( const float NoV, const float NoL )
{
	return saturate( 1.0f / ( 4.0f * ( NoL + NoV - NoL * NoV ) + 0.00001f ) );
}
#endif