#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

#define PI 3.14159265359f
float D_GGX( const float NoH, const float roughness )
{
    float a       = roughness * roughness;
    float a2      = a * a;
    float NoH2    = NoH * NoH;
	
    float num     = a2;
    float denom = ( NoH2 * ( a2 - 1.0 ) + 1.0 );
    denom = PI * denom * denom;
	
    return num / denom;
}

float G_SchlickGGX( const float cosAngle, const float roughness )
{
    const float r = ( roughness + 1.0 );
    const float k = ( r * r ) / 8.0f;

    const float num   = cosAngle;
    const float denom = cosAngle * ( 1.0f - k ) + k;
	
    return ( num / denom );
}

float G_Smith( const float NoV, const float NoL, const float roughness )
{
    const float ggx2 = G_SchlickGGX( NoV, roughness );
    const float ggx1 = G_SchlickGGX( NoL, roughness );	
    return ( ggx1 * ggx2 );
}

vec3 F_Schlick( float cosTheta, vec3 f0 ) {
    return f0 + ( 1.0 - f0 ) * pow( 1.0 - cosTheta, 5.0);
}

float Fd_Lambert() {
    return 1.0 / PI;
}

void main()
{
    const uint materialId = pushConstants.materialId;
    const uint albedoTexId = materials[ materialId ].textureId0;
    const uint normalTexId = materials[ materialId ].textureId1;
    const uint roughnessTexId = materials[ materialId ].textureId2;

    const mat4 modelMat = ubo[ objectId ].model;
    const mat4 viewMat = ubo[ objectId ].view;
    const mat3 invViewMat = mat3( transpose( viewMat ) );
    const vec3 cameraOrigin = -invViewMat * vec3( viewMat[ 3 ][ 0 ], viewMat[ 3 ][ 1 ], viewMat[ 3 ][ 2 ] );
    const vec3 modelOrigin = vec3( modelMat[ 3 ][ 0 ], modelMat[ 3 ][ 1 ], modelMat[ 3 ][ 2 ] );

    const vec3 v = normalize( cameraOrigin.xyz - worldPosition.xyz );
    const vec3 n = normalize( fragNormal ); // normalize( worldPosition.xyz - modelOrigin );
    const vec3 viewDiffuse = dot( v, n ).xxx;

    const vec3 r = reflect( -v, normalize( fragNormal ) );
    const vec4 envColor = vec4( texture( cubeSamplers[ 0 ], vec3( r.x, r.z, r.y ) ).rgb, 1.0f );

    float NoV = abs( dot( n, v ) );

    // Note: Only albedo needs linear conversion
    const vec4 albedo = ( albedoTexId >= 0 ) ? SrgbToLinear( texture( texSampler[ albedoTexId ], fragTexCoord.xy ) ) : vec4 ( 1.0f );
    const vec3 normalTex = ( normalTexId >= 0 ) ? SrgbToLinear( texture( texSampler[ normalTexId ], fragTexCoord.xy ) ).rgb : fragNormal;
    const vec4 roughnessTex = ( roughnessTexId >= 0 ) ? SrgbToLinear( texture( texSampler[ roughnessTexId ], fragTexCoord.xy ) ) : vec4 ( 1.0f );

    const float perceptualRoughness = globals.generic.y * roughnessTex.r;

    float metallic = 0.0f;

    vec3 F0 = vec3( 0.04f ); 
    F0 = albedo.rgb;//mix( F0, albedo, metallic );

    const vec3 ambient = materials[ materialId ].Ka.rgb;
    const vec3 diffuseColor = materials[ materialId ].Kd.rgb;
    const vec3 specularColor = materials[ materialId ].Ks.rgb;
    const float specularPower = materials[ materialId ].Ns;

    vec3 color = vec3( 0.0f, 0.0f, 0.0f );
    for( int i = 0; i < 3; ++i ) {
	    const vec3 l = normalize( lights[ i ].lightPos - worldPosition.xyz );
        const vec3 h = normalize( v + l );

        const float NoL = max( dot( n, l ), 0.0f );
        const float NoH = max( dot( n, h ), 0.0f );
        const float LoH = max( dot( l, h ), 0.0f );

        const float D   = D_GGX( NoH, perceptualRoughness );
        const float G   = G_Smith( NoV, NoL, perceptualRoughness );      
        const vec3 F    = F_Schlick( NoH, F0 ); 

        const vec3 kS = F;
        vec3 kD = vec3( 1.0f ) - kS;
        kD *= 1.0f - metallic;

        const float spotAngle = dot( l, lights[ i ].lightDir );
        const float spotFov = 0.5f;

        vec3 numerator      = D * G * F;
        float denominator   = 4.0f * NoV * NoL + 0.0001;
        vec3 Fr             = numerator / denominator;
               
        const float distance    = length( l );
        const float attenuation = 1.0f / ( distance * distance );
        const float spotFalloff = 1.0f; // * smoothstep( 0.5f, 0.8f, spotAngle );
        const vec3 radiance     = attenuation * spotFalloff * lights[ i ].intensity;

        vec3 diffuse = ( ( kD * albedo.rgb ) / PI + Fr ) * radiance * NoL;
        color += diffuse;
    }
    outColor.rgb = color.rgb; // * Fd_Lambert();
    outColor.a = 1.0f;

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = int( globals.shadowParms.x );
    vec4 lsPosition = ubo[ shadowId ].proj * ubo[ shadowId ].view * vec4( worldPosition.xyz, 1.0f );
    lsPosition.xyz /= lsPosition.w;
    vec2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
    float bias = 0.001f;
    float depth = ( lsPosition.z );

    const ivec2 pixelLocation = ivec2( globals.shadowParms.yz * ndc.xy );
    const float shadowValue = texelFetch( codeSamplers[ shadowMapTexId ], pixelLocation, 0 ).r;
    if( shadowValue < ( depth - bias ) ) // shadowed
    {
        visibility = globals.shadowParms.w;
    }
    //outColor.rgb += vec3( 1.0f, 0.0f, 0.0f ) * pow( 1.0f - NoV, 2.0f );
    outColor.rgb *= visibility;
    outColor.rgb = fragNormal;
}