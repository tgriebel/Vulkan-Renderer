#include "globals.h"

PS_LAYOUT_STANDARD( Texture2D )

PS_Output PSMain( PS_Input input )
{
	PS_Output output = (PS_Output)0;
	output.outColor = float4( 1.0, 0.0, 0.0, 1.0 );
	return output;
}
