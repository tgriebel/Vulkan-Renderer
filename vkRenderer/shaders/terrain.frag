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

    const uint blendId = materials[ materialId ].textureId0;
    const uint textureId0 = materials[ materialId ].textureId1;
    const uint textureId1 = materials[ materialId ].textureId2;

    const float maxHeight = globals.heightMap.x;
    const vec4 blendValue = maxHeight * texture( texSampler[ blendId ], fragTexCoord );
    const vec4 texColor0 = SrgbTolinear( texture( texSampler[ textureId0 ], fragTexCoord ) );
    const vec4 texColor1 = SrgbTolinear( texture( texSampler[ textureId1 ], fragTexCoord ) );
    const vec4 texColor = mix( texColor1, texColor0, smoothstep( 0.0f, 0.4f, blendValue ) );
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
   // outColor.rgb = fragNormal;

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