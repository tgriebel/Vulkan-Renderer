#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )

static const float2 invAtan = float2( 0.1591f, 0.3183f );
float2 SampleSphericalMap( float3 v )
{
	float2 uv = float2( atan2( v.y, -v.x ), asin( -v.z ) );
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
	const uint materialId = pushConstants.materialId;
	const gpuMaterial_t material = materials[materialId];

	const float2 uv = SampleSphericalMap( normalize( input.objectPosition ) );
	const float3 color = globalTextures[ material.textureId[ 0 ] ].Sample( bilinearSamplerWrap, uv ).rgb;

	output.outColor = float4( color, 1.0f );
	return output;
}
