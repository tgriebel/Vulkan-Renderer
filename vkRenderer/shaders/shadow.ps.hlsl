#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
	output.outColor = float4( 1.0, 0.0, 0.0, 1.0 );
	return output;
}
