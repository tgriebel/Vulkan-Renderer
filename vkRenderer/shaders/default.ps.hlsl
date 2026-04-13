#include "globals_hlsl.h"

PS_LAYOUT_BASIC_IO

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
	output.outColor = float4( 1.0f, 0.0f, 1.0f, 1.0f );
	return output;
}
