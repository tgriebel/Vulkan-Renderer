#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

#define PI 3.14159
float D_GGX( const float NoH, const float a )
{
    float a2     = a * a;
    float NoH2   = NoH * NoH;
	
    float nom    = a2;
    float denom  = ( NoH2 * ( a2 - 1.0 ) + 1.0 );
    denom        = PI * denom * denom;
	
    return nom / denom;
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
    const uint textureId = materials[ materialId ].textureId0;

    const mat4 modelMat = ubo[ objectId ].model;
    const mat4 viewMat = ubo[ objectId ].view;
    const mat3 invViewMat = mat3( transpose( viewMat ) );
    const vec3 cameraOrigin = -invViewMat * vec3( viewMat[ 3 ][ 0 ], viewMat[ 3 ][ 1 ], viewMat[ 3 ][ 2 ] );
    const vec3 modelOrigin = vec3( modelMat[ 3 ][ 0 ], modelMat[ 3 ][ 1 ], modelMat[ 3 ][ 2 ] );

    const float perceptualRoughness = 0.1f;
    const float a = perceptualRoughness * perceptualRoughness;

    const vec3 v = normalize( cameraOrigin.xyz - worldPosition.xyz );
    const vec3 n = normalize( fragNormal ); // normalize( worldPosition.xyz - modelOrigin );
    const vec3 viewDiffuse = dot( v, n ).xxx;

    const vec3 r = reflect( -v, normalize( fragNormal ) );
    const vec4 envColor = vec4( texture( cubeSamplers[ 0 ], vec3( r.x, r.z, r.y ) ).rgb, 1.0f );

    float NoV = abs( dot( n, v ) ) + 1e-5;

    const vec4 texColor = SrgbTolinear( texture( texSampler[ textureId ], fragTexCoord.xy ) );
    const vec3 ambient = materials[ materialId ].Ka.rgb;
    const vec3 diffuseColor = materials[ materialId ].Kd.rgb;
    const vec3 specularColor = materials[ materialId ].Ks.rgb;
    const float specularPower = materials[ materialId ].Ns;

    vec3 color = vec3( 0.0f, 0.0f, 0.0f );
    //for( int i = 0; i < 3; ++i ) {
	    const vec3 l = normalize( lights[ 0 ].lightPos - worldPosition.xyz );
        const vec3 h = normalize( v + l );

        const float NoL = clamp( dot( n, l ), 0.0f, 1.0f );
        const float NoH = clamp( dot( n, h ), 0.0f, 1.0f );
        const float LoH = clamp( dot( l, h ), 0.0f, 1.0f );

        const float spotAngle = dot( l, lights[ 0 ].lightDir );
        const float spotFov = 0.5f;

        const float specular = NoH;
        const vec3 specularIntensity = specularColor * pow( specular, max( 1.0f, 64.0f ) );

        vec3 diffuse = diffuseColor * texColor.rgb;
        diffuse *= lights[ 0 ].intensity;// * smoothstep( 0.5f, 0.8f, spotAngle );
        diffuse = ( D_GGX( NoH, a ) * NoL ).xxx;
        color += diffuse;
    //}
    outColor.rgb = color.rgb * Fd_Lambert();
    outColor.a = 1.0f;

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = globals.shadowBaseId;
    vec4 lsPosition = ubo[ shadowId ].proj * ubo[ shadowId ].view * vec4( worldPosition.xyz, 1.0f );
    lsPosition.xyz /= lsPosition.w;
    vec2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
    float bias = 0.001f;
    float depth = ( lsPosition.z );

    const float shadowValue = texture( codeSamplers[ shadowMapTexId ], ndc ).r;
    if( shadowValue < ( depth - bias ) ) // shadowed
    {
   //     visibility = 0.1f;
    }
    outColor.rgb *= visibility;
}