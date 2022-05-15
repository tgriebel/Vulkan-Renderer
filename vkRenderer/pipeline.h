#pragma once

#include "common.h"

enum gfxStateBits_t : uint64_t
{
	GFX_STATE_NONE				= 0,
	GFX_STATE_DEPTH_TEST		= ( 1 << 0 ),
	GFX_STATE_DEPTH_WRITE		= ( 1 << 1 ),
	GFX_STATE_COLOR_MASK		= ( 1 << 2 ),
	GFX_STATE_DEPTH_OP_0		= ( 1 << 3 ),
	GFX_STATE_DEPTH_OP_1		= ( 1 << 4 ),
	GFX_STATE_DEPTH_OP_2		= ( 1 << 5 ),
	GFX_STATE_CULL_MODE_BACK	= ( 1 << 6 ),
	GFX_STATE_CULL_MODE_FRONT	= ( 1 << 7 ),
	GFX_STATE_WIREFRAME			= ( 1 << 10 ),
	GFX_STATE_CULL_BACKFACE		= ( 1 << 11 ),
	GFX_STATE_WIND_CLOCKWISE	= ( 1 << 12 ),
	GFX_STATE_BLEND_ENABLE		= ( 1 << 13 ),
	GFX_STATE_WIREFRAME_ENABLE	= ( 1 << 14 ),
	GFX_STATE_STENCIL_ENABLE	= ( 1 << 15 ),
};

struct pipelineState_t
{
	const char*		tag;
	gfxStateBits_t	stateBits;
	GpuProgram*		shaders;
	viewport_t		viewport;
};

// TODO: replace, scales very poorly
static inline bool operator==( const pipelineState_t& lhs, const pipelineState_t& rhs )
{
	bool equal = ( lhs.viewport.width == rhs.viewport.width );
	equal = equal && ( lhs.viewport.height == rhs.viewport.height );
	equal = equal && ( lhs.viewport.x == rhs.viewport.x );
	equal = equal && ( lhs.viewport.y == rhs.viewport.y );
	equal = equal && ( lhs.viewport.near == rhs.viewport.near );
	equal = equal && ( lhs.viewport.far == rhs.viewport.far );
	
	equal = equal && ( lhs.stateBits == rhs.stateBits );
	equal = equal && ( lhs.shaders == rhs.shaders );

	return equal;
}


struct pipelineObject_t
{
	pipelineState_t		state;
	VkPipeline			pipeline;
	VkPipelineLayout	pipelineLayout;
};


struct pushConstants_t
{
	uint32_t objectId;
	uint32_t materialId;
};

bool GetPipelineObject( pipelineHdl_t hdl, pipelineObject_t** pipelineObject );
void CreateSceneRenderDescriptorSetLayout( VkDescriptorSetLayout& globalLayout );
void CreatePostProcessDescriptorSetLayout( VkDescriptorSetLayout& globalLayout );
void CreateGraphicsPipeline( VkDescriptorSetLayout layout, VkRenderPass pass, const pipelineState_t& state, pipelineHdl_t& pipelineObject );