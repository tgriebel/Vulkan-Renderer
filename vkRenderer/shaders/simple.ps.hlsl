#include "globals_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
	const uint textureId0 = materials[ materialId ].textureId0;

	output.outColor = texSampler[ textureId0 ].Sample( texSamplerSt, input.uv0.xy );
	return output;
}
