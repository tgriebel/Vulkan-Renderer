#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_STANDARD( sampler2D )

void main()
{
    const uint materialId = pushConstants.materialId;
    
    uint textureId = 0;

    const float xm = abs( fragNormal.x );
    const float ym = abs( fragNormal.y );
    const float zm = abs( fragNormal.z );
    const float majorAxis = max( max( xm, ym ), zm );

    if( majorAxis == xm ) {
        textureId = ( sign( fragNormal.x ) > 0.0f ) ? materials[ materialId ].textureId0 : materials[ materialId ].textureId1;
    } else if( majorAxis == ym ) {
        textureId = ( sign( fragNormal.y ) > 0.0f ) ? materials[ materialId ].textureId5 : materials[ materialId ].textureId4;
    } else if( majorAxis == zm ) {
        textureId = ( sign( fragNormal.z ) > 0.0f ) ? materials[ materialId ].textureId2 : materials[ materialId ].textureId3;
    }
	outColor = SrgbToLinear( texture( texSampler[textureId], fragTexCoord.xy ) );
}