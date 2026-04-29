#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
    const uint materialId = pushConstants.materialId;
	const uint textureId0 = materials[ materialId ].textureId[ 0 ];

	output.outColor = globalTextures[ textureId0 ].Sample( bilinearSamplerWrap, input.uv0.xy );
	return output;
}
