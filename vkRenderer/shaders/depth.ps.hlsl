#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

#ifdef USE_MRT
#define VELOCITY_IN_DEPTH
#endif

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
    const uint materialId = pushConstants.materialId;
    
	output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
    
#ifdef VELOCITY_IN_DEPTH
    const float2 current = input.clipPosition.xy / input.clipPosition.w;
    const float2 previous = input.prevClipPosition.xy / input.prevClipPosition.w;

    const float2 velocity = ( current - previous ) * 0.5f;
    
    output.outColor1 = float4( 1.0f, 1.0f, 0.0f, 1.0f );
#endif
    
	return output;
}
