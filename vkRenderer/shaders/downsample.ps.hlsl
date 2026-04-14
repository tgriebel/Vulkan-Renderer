#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

struct ImageShaderTask
{
    float4 generic0;
    float4 generic1;
    float4 generic2;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageShaderTask )

PS_Output PSMain( PS_Input input )
{
    PS_Output output = (PS_Output)0;

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

    const float2 halfTexel = dimensions.zw * 0.5f; // zw is the reciprocal inverse of the dimensions

    float3 result = codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy ).rgb * 4.0f;
    result += codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy + float2( -halfTexel.x, halfTexel.y ) ).rgb;
    result += codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy + float2( halfTexel.x, halfTexel.y ) ).rgb;
    result += codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy + float2( -halfTexel.x, -halfTexel.y ) ).rgb;
    result += codeSamplers[ 0 ].Sample( bilinearSamplerClampEdge, input.uv0.xy + float2( halfTexel.x, -halfTexel.y ) ).rgb;
    result /= 8.0f;

    output.outColor = float4( result, 1.0f );

    return output;
}
