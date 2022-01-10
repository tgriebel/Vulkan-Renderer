#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

void main()
{
    const uint materialId = pushConstants.materialId;
    const uint textureId = materials[ materialId ].textureId0;

    const mat4 viewMat = ubo[ objectId ].view;
    const mat3 invViewMat = mat3( transpose( viewMat ) );
    const vec3 cameraOrigin = -invViewMat * vec3( viewMat[ 3 ][ 0 ], viewMat[ 3 ][ 1 ], viewMat[ 3 ][ 2 ] );

    const vec3 viewVector = normalize( cameraOrigin.xyz - worldPosition.xyz );
    const vec3 viewDiffuse = dot( viewVector, fragNormal.xyz ).xxx;

    const vec4 texColor = SrgbTolinear( texture( texSampler[ textureId ], fragTexCoord ) );
    const vec3 ambient = materials[ materialId ].Ka.rgb;
    const vec3 diffuse = materials[ materialId ].Kd.rgb;
    const vec3 specular = materials[ materialId ].Ks.rgb;
    const float specularPower = materials[ materialId ].Ns;

    vec3 color = vec3( 0.0f, 0.0f, 0.0f );
    for( int i = 0; i < 3; ++i ) {
	    const vec3 lightVector = normalize( lights[ i ].lightPos - worldPosition.xyz );
        const vec3 halfVector = normalize( viewVector + lightVector );

        const float spotAngle = dot( lightVector, lights[ i ].lightDir );
        const float spotFov = 0.5f;

        const vec3 specularIntensity = specular * pow( max( 0.0f, dot( fragNormal, halfVector ) ), specularPower );

        vec3 diffuse = texColor.rgb;
        diffuse *= lights[ i ].intensity * smoothstep( 0.5f, 0.8f, spotAngle );
        diffuse *= max( 0.0f, dot( lightVector, normalize( fragNormal ) ) );
        color += diffuse + specularIntensity;
    }
    outColor.rgb = color;
    outColor.a = 1.0f;

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = globals.shadowBaseId;
    vec4 lsPosition = ubo[ shadowId ].proj * ubo[ shadowId ].view * vec4( worldPosition.xyz, 1.0f );
    lsPosition.xyz /= lsPosition.w;
    vec2 ndc = 0.5f * ( ( lsPosition.xy ) + 1.0f );
    float bias = 0.0001f;
    float depth = ( lsPosition.z );

    const float shadowValue = texture( codeSamplers[ shadowMapTexId ], ndc ).r;
    if( shadowValue < ( depth - bias ) ) // shadowed
    {
        visibility = 0.1f;
    }
    outColor.rgb *= visibility;
}