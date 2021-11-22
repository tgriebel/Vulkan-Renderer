#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD

void main()
{
	const uint objectId = pushConstants.objectId;
    const uint materialId = pushConstants.materialId;

    const uint textureId = materials[ materialId ].textureId0;

    const vec4 texColor = SrgbTolinear( texture( texSampler[ textureId ], fragTexCoord ) );
    outColor = AMBIENT * texColor;
    for( int i = 0; i < 3; ++i ) {
	    const vec3 lightDist = -normalize( lights[ i ].lightPos - worldPosition.xyz );
        const float spotAngle = dot( lightDist, lights[ i ].lightDir );
        const float spotFov = 0.5f;
        vec4 color = texColor;
        // color.rgb *= lights[ i ].intensity * max( 0.0f, dot( lightDist, normalize( fragNormal ) ) );
	    color.rgb *= lights[ i ].intensity * smoothstep( 0.5f, 0.8f, spotAngle );
        outColor += color;
    }

    float visibility = 1.0f;
    const uint shadowMapTexId = 0;
    const uint shadowId = 300;
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