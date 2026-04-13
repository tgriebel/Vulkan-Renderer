#include "globals_hlsl.h"

VS_LAYOUT_STANDARD( Texture2D )

VS_Output VSMain( VS_Input input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	VS_Output output = (VS_Output)0;

	output.objectId = pushConstants.objectId + instanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const view_t view = views[ viewlId ];
	const float4x4 modelMatrix = surfaces[ output.objectId ].model;

	float3 position = input.inPosition;
	output.objectPosition = position;
	output.worldPosition = mul( modelMatrix, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );

	// Tangent-space matrix
	{
		const float normalSign = ( asuint( input.inTangent.x ) & 0x1 ) > 0 ? -1.0f : 1.0f;
		float3 T = normalize( float3( asfloat( asuint( input.inTangent.x ) & ~0x1 ), input.inTangent.yz ) );
		float3 N = normalize( normalSign * cross( input.inTangent, input.inBitangent ) );
		float3 B = normalize( input.inBitangent );
		T = mul( modelMatrix, float4( T, 0.0f ) ).xyz;
		N = mul( modelMatrix, float4( N, 0.0f ) ).xyz;
		B = mul( modelMatrix, float4( B, 0.0f ) ).xyz;
		output.tangent = T;
		output.bitangent = B;
		output.TBN2 = N;
	}

	output.color = input.inColor;
	output.uv0 = input.inTexCoord;
	output.clipPosition = output.pos;

	return output;
}
