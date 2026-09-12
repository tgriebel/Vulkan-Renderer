#include "globals.h"

static const float2 positions[ 6 ] =
{
	float2( -1.0f, -1.0f ),
	float2(  1.0f, -1.0f ),
	float2( -1.0f,  1.0f ),

	float2(  1.0f, -1.0f ),
	float2(  1.0f,  1.0f ),
	float2( -1.0f,  1.0f ),
};


static const float2 uvs[ 6 ] =
{
	float2( 0.0f, 0.0f ),
	float2( 1.0f, 0.0f ),
	float2( 0.0f, 1.0f ),

	float2( 1.0f, 0.0f ),
	float2( 1.0f, 1.0f ),
	float2( 0.0f, 1.0f ),
};


vsToPsInterpolators VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsToPsInterpolators output = (vsToPsInterpolators) 0;
	
    float2 position = positions[ vertexId ].xy;
	
    output.objectPosition	= float3( position.xy, 0.0f );
    output.worldPosition	= float4( position.xy, 0.0f, 1.0f );
	output.pos				= output.worldPosition;
	output.color			= float4( 1.0f, 1.0f, 1.0f, 1.0f );
    output.uv0				= uvs[ vertexId ];
	output.uv1				= float2( 0.0, 0.0 );
	output.normal			= float3( 0.0f, 0.0f, 1.0f );
	output.clipPosition		= output.pos;

	return output;
}
