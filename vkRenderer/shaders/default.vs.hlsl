#include "globals.h"

VS_LAYOUT_BASIC_IO

VS_Output VSMain( VS_Input input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	VS_Output output = (VS_Output)0;

	output.objectPosition	= input.inPosition;
	output.worldPosition	= float4( input.inPosition, 1.0f );
	output.pos				= output.worldPosition;
	output.color			= input.inColor;
	output.uv0				= input.inTexCoord;
	output.normal			= input.inNormal;
	output.clipPosition		= output.pos;

	return output;
}
