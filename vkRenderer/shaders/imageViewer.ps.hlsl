#include "globals.h"

struct ImageViewer
{
	float4	scissorRectUv;
	float4	tint;
	uint	flags;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageViewer )

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;

	const bool isCubeImage = ( ( imageProcess.flags >> 0 ) & 1 ) != 0;

	// The shader uses a clip fullscreen quad so the sample UV needs to be computed from the scissor subsection of the fullscreen quad
	const float2 uv = ( input.uv0.xy - imageProcess.scissorRectUv.xy ) / imageProcess.scissorRectUv.zw;
    const int3 pixelLocation = int3( uv * dimensions.xy, 0 );

	float4 color;
	if ( isCubeImage ) {
		color = localCubemaps[0].Sample( bilinearSamplerClampEdge, float3( uv, 0.0f ) );
	} else {
        color = localTextures[ 0 ].Load( pixelLocation );
    }

	const float4 tint = imageProcess.tint;

	if ( !any( tint ) )
	{
		output.outColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );
		return output;
	}

	const float scalar = dot( color, tint );
	
    const float4 mask = step( 0.00001f, abs( tint ) );
	
    const bool isMonochrome = abs( dot( mask, 1.0.xxxx ) - 1.0f ) < 0.00001f;

	// Single channel isolated: dot gives that channel's value, expand to monochrome
    if ( isMonochrome )
	{
		output.outColor = float4( scalar, scalar, scalar, 1.0f );
	}
	else
	{
		output.outColor   = color * tint;
		output.outColor.a = tint.a > 0.0f ? color.a : 1.0f;
	}

	return output;
}
