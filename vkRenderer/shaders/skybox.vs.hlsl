#include "globals.h"

VS_LAYOUT_STANDARD( Texture2D )

vsToPsInterpolators VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsToPsInterpolators output = (vsToPsInterpolators) 0;

	output.objectId = pushConstants.objectId;
	const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];

	const float maxHeight = 1.0f;
    float3 position = input.inPosition.xyz;
	output.objectPosition = position;
	output.worldPosition = mul( surfaces[ output.objectId ].model, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );
	output.pos.z = 0.0f;
	output.color = input.inColor;
    output.uv0 = input.uv0.xy;
    output.uv1 = input.uv1.xy;
    output.normal = input.inNormal.xyz;
	output.clipPosition = output.pos;

	return output;
}
