#include "globals_hlsl.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
    const uint materialId = pushConstants.materialId;
    output.outColor = float4( materials[ materialId ].Kd.rgb, 1.0f - materials[ materialId ].Tr );
	return output;
}
