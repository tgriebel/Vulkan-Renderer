#pragma once

#include "../asset_types/texture.h"
#include "../globals/common.h"
#include "../render_state/rhi.h"
#include "../asset_types/gpuProgram.h"

class RenderContext;

enum gfxStateBits_t : uint64_t
{
	GFX_STATE_NONE				= 0,
	GFX_STATE_DEPTH_TEST		= ( 1 << 0 ),
	GFX_STATE_DEPTH_WRITE		= ( 1 << 1 ),
	GFX_STATE_COLOR0_MASK		= ( 1 << 2 ),
	GFX_STATE_COLOR1_MASK		= ( 1 << 3 ),
	GFX_STATE_COLOR2_MASK		= ( 1 << 4 ),
	GFX_STATE_DEPTH_OP_0		= ( 1 << 5 ),
	GFX_STATE_DEPTH_OP_1		= ( 1 << 6 ),
	GFX_STATE_DEPTH_OP_2		= ( 1 << 7 ),
	GFX_STATE_CULL_MODE_BACK	= ( 1 << 8 ),
	GFX_STATE_CULL_MODE_FRONT	= ( 1 << 9 ),
	GFX_STATE_WIREFRAME			= ( 1 << 10 ),
	GFX_STATE_CULL_BACKFACE		= ( 1 << 11 ),
	GFX_STATE_WIND_CLOCKWISE	= ( 1 << 12 ),
	GFX_STATE_BLEND_ENABLE		= ( 1 << 13 ),
	GFX_STATE_WIREFRAME_ENABLE	= ( 1 << 14 ),
	GFX_STATE_STENCIL_ENABLE	= ( 1 << 15 ),
	GFX_STATE_COLOR_ENABLE		= ( 1 << 16 ),
	GFX_STATE_BIT_16			= ( 1 << 17 ),
	GFX_STATE_BIT_17			= ( 1 << 18 ),
};
DEFINE_ENUM_OPERATORS( gfxStateBits_t, uint64_t )


struct pipelineState_t
{
	gfxStateBits_t				stateBits;
	imageSamples_t				samplingRate;
	hdl_t						progHdl;
	renderAttachmentBits_t		passBits;
	shaderPermId_t				permSet;
};

class DrawPass;
class ShaderBindSet;


// TODO: Use the same resource model as Image, ImageView, GpuBuffer, etc
// Pipeline should init the actual API object and contain it
class Pipeline
{
private:
	const char*			m_dbgName = "";
	pipelineState_t		m_hashState = {};
	const DrawPass*		m_drawPass = nullptr;
	const GpuProgram*	m_prog = nullptr;
	shaderPermId_t		m_permSet = shaderPermId_t::NONE;

public:

	Pipeline( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet = shaderPermId_t::NONE );

	inline const pipelineState_t& GetHashState() const
	{
		return m_hashState;
	}

	inline const DrawPass* GetDrawPass() const
	{
		return m_drawPass;
	}

	inline const GpuProgram* GetGpuProgram() const
	{
		return m_prog;
	}

	inline const shaderPermId_t& GetShaderPerm() const
	{
		return m_permSet;
	}

	inline const char* GetDebugName() const
	{
		return m_dbgName;
	}
};


struct pipelineObject_t
{
	pipelineState_t		state;
#ifdef USE_VULKAN
	VkPipeline			pipeline;
	VkPipelineLayout	pipelineLayout;
#endif
	const char*			dbgShaderName;
};


struct pushConstants_t
{
	uint32_t objectId;
	uint32_t materialId;
	uint32_t viewId;
};

class ShaderBindSet;

void	ClearPipelineCache();
void	DestroyPipelineCache();
bool	GetPipelineObject( hdl_t hdl, pipelineObject_t** pipelineObject );
hdl_t	FindPipelineObject( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet );
#ifdef USE_VULKAN
void	CreateBindingLayout( ShaderBindSet& parms, VkDescriptorSetLayout& layout );
#endif
hdl_t	CreateGraphicsPipeline( const RenderContext* renderContext, const DrawPass* pass, const Asset<GpuProgram>& prog, const shaderPermId_t permSet = shaderPermId_t::NONE );
void	DestroyGraphicsPipeline( const DrawPass* pass, const Asset<GpuProgram>& prog, const shaderPermId_t permSet = shaderPermId_t::NONE );
void	CreateComputePipeline( const Asset<GpuProgram>& prog );
void	DestroyComputePipeline( const Asset<GpuProgram>& prog );
