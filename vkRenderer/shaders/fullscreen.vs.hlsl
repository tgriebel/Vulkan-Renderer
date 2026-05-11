#include "globals.h"

vsToPsInterpolators VSMain( vsInput_t input, uint vertexId : SV_VertexID, uint instanceIndex : SV_InstanceID )
{
    vsToPsInterpolators output = (vsToPsInterpolators)0;
	
    const float2 uv = float2( ( vertexId << 1 ) & 2, vertexId & 2 );
	
    output.objectPosition = float3( uv.xy * 2.0f - 1.0f, 0.0f );
    output.worldPosition = float4( output.objectPosition.xy, 0.0, 1.0 );
    output.pos = output.worldPosition;
    output.color = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    output.uv0 = uv;
    output.uv1 = float2( 0.0, 0.0 );
    output.normal = float3( 0.0f, 0.0f, 1.0f );
    output.clipPosition = output.pos;

    return output;
}
