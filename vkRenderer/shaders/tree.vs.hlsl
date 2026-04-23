#include "globals.h"

VS_LAYOUT_STANDARD( Texture2D )

vsOutput_t VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsOutput_t output = (vsOutput_t) 0;

	output.objectId = pushConstants.objectId + instanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];

	float3 position = input.inPosition;
	output.objectPosition = position;
	output.worldPosition = mul( surfaces[ output.objectId ].model, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );
	output.color = input.inColor;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
	output.normal = input.inNormal;
	output.clipPosition = output.pos;

	return output;
}
