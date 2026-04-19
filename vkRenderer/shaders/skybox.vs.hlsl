#include "globals.h"

VS_LAYOUT_STANDARD( Texture2D )

VS_Output VSMain( VS_Input input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	VS_Output output = (VS_Output)0;

	output.objectId = pushConstants.objectId;
	const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];

	const float maxHeight = 1.0f;
	float3 position = input.inPosition;
	output.objectPosition = position;
	output.worldPosition = mul( surfaces[ output.objectId ].model, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );
	output.pos.z = 0.0f;
	output.color = input.inColor;
	output.uv0 = input.inTexCoord;
	output.normal = input.inNormal;
	output.clipPosition = output.pos;

	return output;
}
