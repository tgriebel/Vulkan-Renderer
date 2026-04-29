#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
    const uint materialId = pushConstants.materialId;

	output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
	return output;
}
