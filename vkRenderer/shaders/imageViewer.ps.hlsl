#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

struct ImageViewer
{
	float4	scissorRectUv;
	uint	flags;
};

PS_LAYOUT_IMAGE_PROCESS( Texture2D, ImageViewer )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;

	const bool isCubeImage = ( ( imageProcess.flags >> 0 ) & 1 ) != 0;

	float2 uv = ( input.uv0.xy - imageProcess.scissorRectUv.xy ) / imageProcess.scissorRectUv.zw;

	if( isCubeImage ) {
		output.outColor = codeCubeSamplers[0].Sample( bilinearSamplerClampEdge, float3(uv, 0.0f));
	} else {
		output.outColor = codeSamplers[0].Sample( bilinearSamplerWrap, uv);
	}

	return output;
}
