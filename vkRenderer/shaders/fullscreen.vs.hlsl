#include "globals.h"

static const float2 positions[ 3 ] = {
	float2( -1.0f, -1.0f ),
	float2( 3.0f, -1.0f ),
	float2( -1.0f, 3.0f )
};

static const float2 uvs[ 3 ] = {
	float2( 0.0f, 0.0f ),
	float2( 2.0f, 0.0f ),
	float2( 0.0f, 2.0f )
};

// TODO: replace with this
// float2( ( vertexId << 1 ) & 2, vertexId & 2 );
// float4( output.uv0.xy * 2.0f + -1.0f, 0.0f, 1.0f );

vsOutput_t VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
	vsOutput_t output = (vsOutput_t) 0;

	output.objectPosition	= float3( positions[ vertexId ].xy, 0.0f );
	output.worldPosition	= float4( positions[ vertexId ].xy, 0.0, 1.0 );
	output.pos				= output.worldPosition;
	output.color			= float4( 1.0f, 1.0f, 1.0f, 1.0f );
	output.uv0				= float4( uvs[ vertexId ], 0.0, 0.0 );
	output.normal			= float3( 0.0f, 0.0f, 1.0f );
	output.clipPosition		= output.pos;

	return output;
}
