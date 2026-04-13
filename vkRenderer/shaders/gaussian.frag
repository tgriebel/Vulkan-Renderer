#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "globals.h"

PS_LAYOUT_BASIC_IO

struct GaussianProcess
{
    uint dummy;
};

PS_LAYOUT_IMAGE_PROCESS( sampler2D, GaussianProcess )

const uint weightCount = 5;
const float weights[ weightCount ] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

void main()
{
    const bool horizontal = ( pass == 0 ) ? true : false;
    const uint texId = ( pass == 0 ) ? 0 : previousImageId;
    
    const float lod = 0.0f;

    vec2 offset = dimensions.zw;
    outColor = vec4( textureLod( codeSamplers[ 0 ], fragTexCoord.xy, lod ).rgb * weights[ 0 ], 1.0f );

    if ( horizontal )
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            outColor.rgb += textureLod( codeSamplers[ texId ], fragTexCoord.xy + vec2( offset.x * i, 0.0 ), lod ).rgb * weights[ i ];
            outColor.rgb += textureLod( codeSamplers[ texId ], fragTexCoord.xy - vec2( offset.x * i, 0.0 ), lod ).rgb * weights[ i ];
        }
    }
    else
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            outColor.rgb += textureLod( codeSamplers[ texId ], fragTexCoord.xy + vec2( 0.0, offset.y * i ), lod ).rgb * weights[ i ];
            outColor.rgb += textureLod( codeSamplers[ texId ], fragTexCoord.xy - vec2( 0.0, offset.y * i ), lod ).rgb * weights[ i ];
        }
    }
    outColor.a = 1.0f;
}