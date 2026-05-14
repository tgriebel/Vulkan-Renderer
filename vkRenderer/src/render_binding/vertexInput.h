#include <stdint.h>
#include "../globals/common.h"
#include <SysCore/array.h>

static const uint32_t MaxVertexAttribs = 7;

struct vertexAttribute_t
{
	uint32_t							slot;
#if defined( USE_VULKAN )
	VkVertexInputAttributeDescription	desc;
#endif
};


using VertexDescription = Array<vertexAttribute_t, MaxVertexAttribs>;

VertexDescription GetVertexAttributeDescriptions();