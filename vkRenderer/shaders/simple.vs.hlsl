#include "globals.h"

VS_LAYOUT_STANDARD( Texture2D )

vsToPsInterpolators VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsToPsInterpolators output = (vsToPsInterpolators) 0;

	output.objectId = pushConstants.objectId + instanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];
	const float4x4 modelMatrix = surfaces[ output.objectId ].model;

	const float3 position = input.inPosition;
    const float4 wsPosition = mul( modelMatrix, float4( position, 1.0f ) );
	
    const float4 clipPos = mul( view.projMat, mul( view.viewMat, wsPosition ) );
    const float4 prevClipPos = mul( view.prevViewProjMat, wsPosition );
	
	output.objectPosition = position;
    output.worldPosition = wsPosition;
    output.pos = clipPos;
	
	// Tangent-space matrix
	{
		const float normalSign = ( asuint( input.inTangent.x ) & 0x1 ) > 0 ? -1.0f : 1.0f;
		float3 T = float3( asfloat( asuint( input.inTangent.x ) & ~0x1 ), input.inTangent.yz );
		float3 N = input.inNormal;
		float3 B = normalSign * cross( N, T );
		
		T = mul( modelMatrix, float4( T, 0.0f ) ).xyz;
		N = mul( modelMatrix, float4( N, 0.0f ) ).xyz;
		B = mul( modelMatrix, float4( B, 0.0f ) ).xyz;
		
		output.tangent = T;
		output.bitangent = B;
		output.TBN2 = N;
	}

	output.color = input.inColor;
	output.uv0 = input.uv0;
	output.uv1 = input.uv1;
    output.clipPosition = clipPos;
    output.prevClipPosition = prevClipPos;

	return output;
}
