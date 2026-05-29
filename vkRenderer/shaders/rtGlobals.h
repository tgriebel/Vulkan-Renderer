#include "globals.h"

struct hitPayload_t
{
	float3	color;
	float	weight;
	int		depth;
};


struct rtPushConstants_t
{
	float dummy;
};


BIND_INLINE rtPushConstants_t pushConst;
// Texture array for material textures
[[vk::binding( BindingPoints::eTextures, 0 )]] Sampler2D textures[];
// Top-level acceleration structure containing the scene geometry
[[vk::binding( BindingPoints::eTlas, 1 )]] RaytracingAccelerationStructure topLevelAS;
// Output image where the final rendered result will be stored
[[vk::binding( BindingPoints::eOutImage, 1 )]] RWTexture2D<float4> outImage;

