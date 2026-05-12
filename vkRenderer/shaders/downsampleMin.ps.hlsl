#include "globals.h"

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, ImageShaderTask )

psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

	// X: Destination pixel in output MIP
	// s*: Samples from input MIP
	//
	// +-------- +-------- +
	// |         |         |
	// |   s0    |   s1    |
	// |         |         |
	// +---------X---------+
	// |         |         |
	// |   s2    |   s3    |
	// |         |         |
	// +---------+---------+

    const float2 halfTexel = dimensions.zw * 0.5f;

    const float4 s0 = localTextures[ 0 ].Sample( nearestSampler, input.uv0.xy + float2( -halfTexel.x,  halfTexel.y ) );
    const float4 s1 = localTextures[ 0 ].Sample( nearestSampler, input.uv0.xy + float2(  halfTexel.x,  halfTexel.y ) );
    const float4 s2 = localTextures[ 0 ].Sample( nearestSampler, input.uv0.xy + float2( -halfTexel.x, -halfTexel.y ) );
    const float4 s3 = localTextures[ 0 ].Sample( nearestSampler, input.uv0.xy + float2(  halfTexel.x, -halfTexel.y ) );

    output.outColor = min( min( s0, s1 ), min( s2, s3 ) );

    return output;
}
