#include "globals.h"

vsOutput_t VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsOutput_t output = (vsOutput_t) 0;

	output.objectPosition	= input.inPosition;
	output.worldPosition	= float4( input.inPosition, 1.0f );
	output.pos				= output.worldPosition;
	output.color			= input.inColor;
	output.uv0				= input.inTexCoord;
	output.normal			= input.inNormal;
	output.clipPosition		= output.pos;

	return output;
}
