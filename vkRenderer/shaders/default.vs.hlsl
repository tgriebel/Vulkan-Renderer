#include "globals.h"

vsToPsInterpolators VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsToPsInterpolators output = (vsToPsInterpolators) 0;

    output.objectPosition	= input.inPosition.xyz;
	output.worldPosition	= float4( input.inPosition.xyz, 1.0f );
	output.pos				= output.worldPosition;
    output.color			= input.inColor;
    output.uv0				= input.uv0.xy;
    output.uv1				= input.uv1.xy;
	output.normal			= input.inNormal.xyz;
	output.clipPosition		= output.pos;

	return output;
}
