#include "globals.h"

VS_LAYOUT_STANDARD( Texture2D )

float3x3 GetTerrainTangent( float2 uv )
{
	int2 texDim = GetTextureSize( globalTextures[2], 0 );

	const float maxHeight = 1.0f;
	float gridSize = 0.01f;
	int cx = int( uv.x * texDim.x );
	int cy = int( uv.y * texDim.y );
	int x0 = cx - 1;
	int x1 = cx + 1;
	int y0 = cy - 1;
	int y1 = cy + 1;

	float dzdx = globalTextures[2].Load( int3( x1, cy, 0 ) ).r - globalTextures[2].Load( int3( x0, cy, 0 ) ).r;
	float dzdy = globalTextures[2].Load( int3( cx, y1, 0 ) ).r - globalTextures[2].Load( int3( cx, y0, 0 ) ).r;

	dzdx *= maxHeight / gridSize;
	dzdy *= maxHeight / gridSize;

	float3 bx = normalize( float3( 2.0f, 0.0f, dzdx ) );
	float3 by = normalize( float3( 0.0f, 2.0f, dzdy ) );

	return float3x3( bx, by, float3( 0.0f, 0.0f, 1.0f ) );
}

vsToPsInterpolators VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsToPsInterpolators output = (vsToPsInterpolators) 0;

	output.objectId = pushConstants.objectId + instanceIndex;
	const uint materialId = pushConstants.materialId;
	const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[viewlId];

	const uint heightMapId = materials[ materialId ].textureId[ 0 ];
	const float heightMapValue = globalTextures[ heightMapId ].SampleLevel( bilinearSamplerWrap, input.uv0.xy, 0 ).r;

	const float maxHeight = globals.generic.x;
	float3 position = input.inPosition;
	position.z += maxHeight * heightMapValue;
	output.objectPosition = position;
	output.worldPosition = mul( surfaces[ output.objectId ].model, float4( position, 1.0f ) );
	output.pos = mul( view.projMat, mul( view.viewMat, output.worldPosition ) );
	output.color = input.inColor;
	output.uv0 = input.uv0;
	output.uv1 = input.uv1;

	float3x3 modelMat3 = (float3x3)surfaces[ output.objectId ].model;
	float3x3 tangentMat = GetTerrainTangent( input.uv0.xy );
	float3 wt0 = mul( modelMat3, tangentMat[0] );
	float3 wt1 = mul( modelMat3, tangentMat[1] );
	output.normal = normalize( cross( wt0, wt1 ) );
	output.clipPosition = output.pos;

	return output;
}
