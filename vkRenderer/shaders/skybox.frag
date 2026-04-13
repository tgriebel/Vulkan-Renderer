#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"
#include "color.h"
#include "util.h"

PS_LAYOUT_STANDARD( sampler2D )

void main()
{
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

    const material_t material = materialUbo.materials[ materialId ];
    const view_t view = viewUbo.views[ viewlId ];

#ifdef USE_CUBE_SAMPLER
    const vec3 viewVector = normalize( objectPosition );
    const vec3 skyColor = texture( cubeSamplers[ material.textureId0 ], CubeVector( viewVector ) ).rgb;
    outColor.rgb = SrgbToLinear( skyColor );
#else
    const float xm = abs( fragNormal.x );
    const float ym = abs( fragNormal.y );
    const float zm = abs( fragNormal.z );
    const float majorAxis = max( max( xm, ym ), zm );

    uint textureId = 0;

    if( majorAxis == xm ) {
        textureId = ( sign( fragNormal.x ) > 0.0f ) ? material.textureId0 : material.textureId1;
    } else if( majorAxis == ym ) {
        textureId = ( sign( fragNormal.y ) > 0.0f ) ? material.textureId5 : material.textureId4;
    } else if( majorAxis == zm ) {
        textureId = ( sign( fragNormal.z ) > 0.0f ) ? material.textureId2 : material.textureId3;
    }
	outColor = SrgbToLinear( texture( texSampler[textureId], fragTexCoord.xy ) );
#endif
    outColor.a = 1.0f;
}