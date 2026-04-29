#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
	const uint textureId0 = materials[ materialId ].textureId[ 0 ];

	output.outColor = globalTextures[ textureId0 ].Sample( bilinearSamplerWrap, input.uv0.xy );
	return output;
}
