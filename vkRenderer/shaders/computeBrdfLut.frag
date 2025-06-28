#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"
#include "color.h"
#include "light.h"

PS_LAYOUT_BASIC_IO

struct BrdfLutConstants
{
    float dummy;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, BrdfLutConstants )

// https://learnopengl.com/PBR/IBL/Specular-IBL
vec2 IntegrateBRDF( const float NoV, const float roughness )
{
    vec3 V;
    V.x = sqrt( 1.0 - NoV * NoV );
    V.y = 0.0;
    V.z = NoV;

    float A = 0.0;
    float B = 0.0;

    vec3 N = vec3( 0.0, 0.0, 1.0 );

    const uint SAMPLE_COUNT = 1024u;
    for ( uint i = 0u; i < SAMPLE_COUNT; ++i )
    {
        vec2 Xi = Hammersley( i, SAMPLE_COUNT );
        vec3 H = ImportanceSampleGGX( Xi, N, roughness );
        vec3 L = normalize( 2.0 * dot( V, H ) * H - V );

        float NoL = max( L.z, 0.0 );
        float NoH = max( H.z, 0.0 );
        float VoH = max( dot( V, H ), 0.0 );
        float NoV = max( dot( N, V ), 0.0 );

        if ( NoL > 0.0 )
        {
            float G = G_Smith( NoV, max( dot( N, L ), 0.0 ), roughness );
            float G_Vis = ( G * VoH ) / ( NoH * NoV );
            float Fc = pow( 1.0 - VoH, 5.0 );

            A += ( 1.0 - Fc ) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float( SAMPLE_COUNT );
    B /= float( SAMPLE_COUNT );
    return vec2( A, B );
}

void main()
{
    vec2 integratedBRDF = IntegrateBRDF( fragTexCoord.x, fragTexCoord.y );
    outColor.r = integratedBRDF.x;
    outColor.g = integratedBRDF.y;
    outColor.b = 0.0f;
    outColor.a = 1.0f;
}