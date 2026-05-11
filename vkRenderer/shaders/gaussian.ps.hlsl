#include "globals.h"

struct GaussianProcess
{
    uint dummy;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, GaussianProcess )

static const uint weightCount = 5;
static const float weights[ 5 ] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const bool horizontal = ( pass == 0 ) ? true : false;
    const uint texId = ( pass == 0 ) ? 0 : previousImageId;

    const float lod = 0.0f;

    float2 offset = dimensions.zw;
    output.outColor = float4( localTextures[ 0 ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy, lod ).rgb * weights[ 0 ], 1.0f );

    if ( horizontal )
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            output.outColor.rgb += localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy + float2( offset.x * i, 0.0 ), lod ).rgb * weights[ i ];
            output.outColor.rgb += localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy - float2( offset.x * i, 0.0 ), lod ).rgb * weights[ i ];
        }
    }
    else
    {
        for ( uint i = 1; i < weightCount; ++i )
        {
            output.outColor.rgb += localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy + float2( 0.0, offset.y * i ), lod ).rgb * weights[ i ];
            output.outColor.rgb += localTextures[ texId ].SampleLevel( bilinearSamplerClampEdge, input.uv0.xy - float2( 0.0, offset.y * i ), lod ).rgb * weights[ i ];
        }
    }
    output.outColor.a = 1.0f;

    return output;
}
