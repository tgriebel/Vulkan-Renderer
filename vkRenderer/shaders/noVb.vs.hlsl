#include "globals.h"

VS_OUT

static const float2 positions[ 4 ] = {
	float2( 0.0f, 0.0f ),
	float2( 1.0f, 0.0f ),
	float2( 0.0f, 1.0f ),
	float2( 1.0f, 1.0f )
};

static const float2 uvs[ 4 ] = {
	float2( 0.0f, 0.0f ),
	float2( 1.0f, 0.0f ),
	float2( 0.0f, 1.0f ),
	float2( 1.0f, 1.0f )
};

VS_Output VSMain( uint vertexId : SV_VertexID )
{
	VS_Output output = (VS_Output)0;

	output.uv0		= float4( uvs[ vertexId ], 0.0, 0.0 );
	output.pos				= float4( positions[ vertexId ], 0.0f, 0.0f );

	output.objectPosition	= output.pos.xyz;
	output.worldPosition	= output.pos;
	output.color		= float4( 1.0f, 1.0f, 1.0f, 1.0f );
	output.normal		= float3( 0.0f, 0.0f, 1.0f );
	output.clipPosition		= output.pos;

	return output;
}
