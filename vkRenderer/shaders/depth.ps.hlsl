#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;

	output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
	return output;
}
