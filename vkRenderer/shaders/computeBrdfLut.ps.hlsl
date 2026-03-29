#include "globals_hlsl.h"
#include "color_hlsl.h"
#include "light_hlsl.h"

PS_LAYOUT_BASIC_IO

struct BrdfLutConstants
{
    float dummy;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, BrdfLutConstants )

// https://learnopengl.com/PBR/IBL/Specular-IBL
float G_SchlickGGX_BRDF_LUT( float NdotV, float roughness )
{
    // note that we use a different k for IBL
    float a = roughness;
    float k = ( a * a ) / 2.0;

    float nom = NdotV;
    float denom = NdotV * ( 1.0 - k ) + k;

    return nom / denom;
}

float G_Smith_BRDF_LUT( float3 N, float3 V, float3 L, float roughness )
{
    float NdotV = max( dot( N, V ), 0.0 );
    float NdotL = max( dot( N, L ), 0.0 );
    float ggx2 = G_SchlickGGX_BRDF_LUT( NdotV, roughness );
    float ggx1 = G_SchlickGGX_BRDF_LUT( NdotL, roughness );

    return ggx1 * ggx2;
}

// https://learnopengl.com/PBR/IBL/Specular-IBL
float2 IntegrateBRDF( const float NoV, const float roughness )
{
    float3 V;
    V.x = sqrt( 1.0 - NoV * NoV );
    V.y = 0.0;
    V.z = NoV;

    float A = 0.0;
    float B = 0.0;

    float3 N = float3( 0.0, 0.0, 1.0 );

    const uint SAMPLE_COUNT = 1024u;
    for ( uint i = 0u; i < SAMPLE_COUNT; ++i )
    {
        float2 Xi = Hammersley( i, SAMPLE_COUNT );
        float3 H = ImportanceSampleGGX( Xi, N, roughness );
        float3 L = normalize( 2.0 * dot( V, H ) * H - V );

        float NoL = max( L.z, 0.0 );
        float NoH = max( H.z, 0.0 );
        float VoH = max( dot( V, H ), 0.0 );

        if ( NoL > 0.0 )
        {
            float G = G_Smith_BRDF_LUT( N, V, L, roughness );
            float G_Vis = ( G * VoH ) / ( NoH * NoV );
            float Fc = pow( 1.0 - VoH, 5.0 );

            A += ( 1.0 - Fc ) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float( SAMPLE_COUNT );
    B /= float( SAMPLE_COUNT );
    return float2( A, B );
}

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

    float2 integratedBRDF = IntegrateBRDF( input.fragTexCoord.x, 1.0f - input.fragTexCoord.y );
    output.outColor.r = integratedBRDF.x;
    output.outColor.g = integratedBRDF.y;
    output.outColor.b = 0.0f;
    output.outColor.a = 1.0f;

    return output;
}
