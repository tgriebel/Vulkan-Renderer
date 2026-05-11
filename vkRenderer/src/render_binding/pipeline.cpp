#include "../../stdafx.h"

#include "pipeline.h"
#include "../render_core/renderer.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../render_binding/bindings.h"
#include "shaderBinding.h"
#include "../scene/sceneBase.h"
#include "../asset_types/assetLib.h"

static std::unordered_map< uint64_t, pipelineObject_t > s_pipelineLib;
static std::unordered_map< uint64_t, std::set<pipelineState_t> > s_progToPipelines;

static const uint32_t MaxVertexAttribs = 7;
static std::array<VkVertexInputAttributeDescription, MaxVertexAttribs> GetVertexAttributeDescriptions()
{
	uint32_t attribId = 0;

	std::array<VkVertexInputAttributeDescription, MaxVertexAttribs> attributeDescriptions{ };
	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, inPosition );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, inColor );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, inNormal );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, inTangent );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, inBitangent );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, uv0 );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, uv1 );
	++attribId;

	assert( attribId == MaxVertexAttribs );

	return attributeDescriptions;
}


void ClearPipelineCache()
{
	s_pipelineLib.clear();
}


void DestroyPipelineCache()
{
	for ( auto it = s_pipelineLib.begin(); it != s_pipelineLib.end(); ++it )
	{
		vkDestroyPipeline( context.device, it->second.pipeline, nullptr );
		vkDestroyPipelineLayout( context.device, it->second.pipelineLayout, nullptr );
	}
	s_pipelineLib.clear();
}


static hdl_t GetGfxPipelineStateHash( const pipelineState_t& state )
{
	return Hash( reinterpret_cast<const uint8_t*>( &state ), offsetof( pipelineState_t, prog ) );
}


static hdl_t GetComputePipelineStateHash( const pipelineState_t& state )
{
	return Hash( reinterpret_cast<const uint8_t*>( &state.progHdl ), sizeof( state.progHdl ) );
}


hdl_t GetComputePipelineStateHandle( const Asset<GpuProgram>& progAsset )
{
	return Hash( reinterpret_cast<const uint8_t*>( &progAsset.Handle() ), sizeof( progAsset.Handle() ) );
}


pipelineState_t CreateGfxState( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	pipelineState_t state {};

	state.stateBits = pass->StateBits();
	state.samplingRate = pass->SampleRate();
	state.progHdl = progAsset.Handle();
	state.passBits = pass->GetFrameBuffer()->GetAttachmentBits();
	state.permSet = permSet;
	state.prog = &progAsset.Get();
	state.dbgProgName = progAsset.GetName().c_str();

	if( pass->GetFrameBuffer()->ColorLayerCount() > 1 ) {
		SetFlags( state.permSet, shaderPermId_t::MRT );
	} else {
		ClearFlags( state.permSet, shaderPermId_t::MRT );
	}

	return state;
}


pipelineState_t CreateComputeState( const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state{};
	state.progHdl = progAsset.Handle();
	state.prog = &progAsset.Get();
	state.dbgProgName = progAsset.GetName().c_str();

	return state;
}


bool GetPipelineObject( hdl_t hdl, pipelineObject_t** pipelineObject )
{
	auto it = s_pipelineLib.find( hdl.Get() );
	if ( it != s_pipelineLib.end() ) {
		*pipelineObject = &it->second;
		return true;
	}
	*pipelineObject = nullptr;
	return false;
}


hdl_t FindPipelineObject( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	const pipelineState_t state = CreateGfxState( pass, progAsset, permSet );

	const hdl_t pipelineHdl = GetGfxPipelineStateHash( state );

	auto it = s_pipelineLib.find( pipelineHdl.Get() );
	if ( it != s_pipelineLib.end() ) {
		return pipelineHdl;
	}
	return INVALID_HDL;
}


void DestoryAllPipelines( const Asset<GpuProgram>& progAsset )
{
	auto pipelineSetIt = s_progToPipelines.find( progAsset.Handle().Get() );

	if( pipelineSetIt == s_progToPipelines.end() ) {
		return;
	}

	std::set<pipelineState_t>& pipelineHandles = pipelineSetIt->second;

	for( auto pipelineState : pipelineHandles )
	{
		const hdl_t pipelineHdl = GetGfxPipelineStateHash( pipelineState );

		auto pipelineIt = s_pipelineLib.find( pipelineHdl.Get() );
		if( pipelineIt == s_pipelineLib.end() ){
			continue;
		}
		vkDestroyPipeline( context.device, pipelineIt->second.pipeline, nullptr );
		vkDestroyPipelineLayout( context.device, pipelineIt->second.pipelineLayout, nullptr );

		pipelineIt->second.pipeline = VK_NULL_HANDLE;
		pipelineIt->second.pipelineLayout = VK_NULL_HANDLE;
	}
	pipelineHandles.clear();
}


void DestroyGraphicsPipeline( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	const pipelineState_t state = CreateGfxState( pass, progAsset, permSet );

	const hdl_t pipelineHdl = GetGfxPipelineStateHash( state );

	auto it = s_pipelineLib.find( pipelineHdl.Get() );
	if ( it == s_pipelineLib.end() ) {
		return;
	}
	vkDestroyPipeline( context.device, it->second.pipeline, nullptr );
	vkDestroyPipelineLayout( context.device, it->second.pipelineLayout, nullptr );

	s_pipelineLib.erase( it );
}


hdl_t CreateGraphicsPipeline( const DrawPass* pass, const Asset<GpuProgram>& progAsset, const shaderPermId_t permSet )
{
	const pipelineState_t state = CreateGfxState( pass, progAsset, permSet );
	const hdl_t pipelineHdl = GetGfxPipelineStateHash( state );
	return CreateGraphicsPipeline( pass, pipelineHdl, state );
}


hdl_t CreateGraphicsPipeline( const DrawPass* pass, const hdl_t pipelineHdl, const pipelineState_t& state )
{
	auto it = s_pipelineLib.find( pipelineHdl.Get() );
	const bool found = ( it != s_pipelineLib.end() );

	pipelineObject_t pipelineObject{};
	
	if( found )
	{
		pipelineObject = it->second;
		if( pipelineObject.pipeline != VK_NULL_HANDLE ) {
			return pipelineHdl;
		}
	}
	else
	{
		pipelineObject.state = state;
		pipelineObject.prog = state.prog;
		pipelineObject.dbgProgName = state.dbgProgName;
	}

	assert( pipelineObject.prog == state.prog );
	assert( pipelineHdl != INVALID_HDL );

	const GpuProgram& prog = *state.prog;

	const uint32_t permIndex = static_cast<uint32_t>( state.permSet );

	assert( prog.shaderCount == 2 );

	auto shaderMapItVs = prog.shaderBins[ 0 ].find( permIndex );
	if( shaderMapItVs == prog.shaderBins[ 0 ].end() ) {
		return INVALID_HDL;
	}

	const ShaderBin& vsBin = shaderMapItVs->second;
	assert( vsBin.type == shaderType_t::VERTEX );

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{ };
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vsBin.vk_shader;
	vertShaderStageInfo.pName = "VSMain";

	auto shaderMapItPs = prog.shaderBins[ 1 ].find( permIndex );
	if( shaderMapItPs == prog.shaderBins[ 1 ].end() ) {
		return INVALID_HDL;
	}

	const ShaderBin& psBin = shaderMapItPs->second;
	assert( psBin.type == shaderType_t::PIXEL );

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{ };
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = psBin.vk_shader;
	fragShaderStageInfo.pName = "PSMain";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescription{ };
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof( vsInput_t );
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	auto attributeDescriptions = GetVertexAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{ };
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	if( HasFlags( prog.flags, shaderFlags_t::NO_VERTEX_BUFFER ) )
	{
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	}
	else
	{
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() );
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{ };
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{ };
	viewport.x = static_cast<float>( pass->GetViewport().x );
	viewport.y = static_cast<float>( pass->GetViewport().y );
	viewport.width = static_cast<float>( pass->GetViewport().width );
	viewport.height = static_cast<float>( pass->GetViewport().height );
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{ };
	scissor.offset = { pass->GetViewport().x, pass->GetViewport().y };
	scissor.extent = { pass->GetViewport().width, pass->GetViewport().height };

	VkPipelineViewportStateCreateInfo viewportState{ };
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	uint32_t cullBits = VK_CULL_MODE_NONE;
	cullBits |= ( ( state.stateBits & GFX_STATE_CULL_MODE_BACK ) != 0 ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	cullBits |= ( ( state.stateBits & GFX_STATE_CULL_MODE_FRONT ) != 0 ) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;

	VkPipelineRasterizationStateCreateInfo rasterizer{ };
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = ( ( state.stateBits & GFX_STATE_WIREFRAME_ENABLE ) != 0 ) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = cullBits;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{ };
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = ForceDisableMSAA ? VK_FALSE : VK_TRUE;
	multisampling.rasterizationSamples = vk_GetSampleCount( state.samplingRate );
	multisampling.minSampleShading = 0.25f;
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	const VkColorComponentFlags allColorFlags = ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT );

	VkColorComponentFlags colorFlags[3] = {};
	if ( ( state.stateBits & GFX_STATE_COLOR0_MASK ) == 0 ) {
		colorFlags[ 0 ] = allColorFlags;
	}
	if ( ( state.stateBits & GFX_STATE_COLOR1_MASK ) == 0 ) {
		colorFlags[ 1 ] = allColorFlags;
	}
	if ( ( state.stateBits & GFX_STATE_COLOR2_MASK ) == 0 ) {
		colorFlags[ 2 ] = allColorFlags;
	}

	const bool blendEnable = ( ( state.stateBits & GFX_STATE_BLEND_ENABLE ) != 0 );

	const uint32_t colorAttachmentCount = pass->GetFrameBuffer()->ColorLayerCount();
	assert( colorAttachmentCount <= 3 );

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	colorBlendAttachments.resize( colorAttachmentCount );

	for ( uint32_t i = 0; i < colorAttachmentCount; ++i )
	{
		colorBlendAttachments[ i ].colorWriteMask      = colorFlags[ i ];
		colorBlendAttachments[ i ].blendEnable         = blendEnable ? VK_TRUE : VK_FALSE;
		colorBlendAttachments[ i ].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachments[ i ].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachments[ i ].colorBlendOp        = VK_BLEND_OP_ADD;
		colorBlendAttachments[ i ].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachments[ i ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachments[ i ].alphaBlendOp        = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{ };
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = static_cast<uint32_t>( colorBlendAttachments.size() );
	colorBlending.pAttachments = colorBlendAttachments.data();

	uint32_t dynamicStatesCount = 0;
	VkDynamicState dynamicStates[4];
	if( ( ( state.stateBits & GFX_STATE_STENCIL_ENABLE ) != 0 ) ) {
		dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
	}
	dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_SCISSOR;
	dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_LINE_WIDTH;

	VkPipelineDynamicStateCreateInfo dynamicState{ };
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamicStatesCount;
	dynamicState.pDynamicStates = dynamicStates;
	
	VkDescriptorSetLayout layouts[GpuProgram::MaxBindSets];
	for( uint32_t i = 0; i < prog.bindsetCount; ++i ) {
		layouts[ i ] = prog.bindsets[ i ]->GetVkObject();
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.setLayoutCount = prog.bindsetCount;
	
	pipelineLayoutInfo.pushConstantRangeCount = 1;

	VkPushConstantRange pushRanges{};
	if( HasFlags( prog.flags, shaderFlags_t::IMAGE_SHADER ) )
	{
		pushRanges.offset = 0;
		pushRanges.size = sizeof( gpuImageShaderPushConstants_t );
		pushRanges.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineLayoutInfo.pPushConstantRanges = &pushRanges;
	}
	else
	{
		pushRanges.offset = 0;
		pushRanges.size = sizeof( gpuPushConstants_t );
		pushRanges.stageFlags = VK_SHADER_STAGE_ALL;
		pipelineLayoutInfo.pPushConstantRanges = &pushRanges;
	}

	VK_CHECK_RESULT( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, ( "Pipeline Layout (GFX): < " + vsBin.binName + " | " + psBin.binName + " >" ).c_str() );

	const bool depthTestEnable = ( ( state.stateBits & GFX_STATE_DEPTH_TEST ) != 0 );
	const bool depthWriteEnable = ( ( state.stateBits & GFX_STATE_DEPTH_WRITE ) != 0 );

	VkCompareOp blendOp;
	if( ( state.stateBits & GFX_STATE_DEPTH_OP_0 ) != 0 ) {
		blendOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	} else {
		blendOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil{ };
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = depthWriteEnable && !blendEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = blendOp;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional

	const bool stencilEnable = ( ( state.stateBits & GFX_STATE_STENCIL_ENABLE ) != 0 );

	if ( stencilEnable && ( blendEnable == false ) )
	{
		depthStencil.stencilTestEnable = VK_TRUE;
		depthStencil.back.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.depthFailOp = VK_STENCIL_OP_REPLACE;
		depthStencil.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencil.back.compareMask = 0xFF;
		depthStencil.back.writeMask = 0xFF;
		depthStencil.back.reference = 0;
		depthStencil.front = depthStencil.back;
	}
	else
	{
		depthStencil.back.compareOp = VK_COMPARE_OP_NEVER;
		depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencil.front = depthStencil.back;
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{ };
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = ( colorAttachmentCount > 0 ) ? 2 : 1;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineObject.pipelineLayout;
	const renderPassAttachmentMask_t attachMask = pass->GetFrameBuffer()->GetAttachmentMask();
	const renderPassAttachmentBits_t* colorBits[ 3 ] = { &state.passBits.color0, &state.passBits.color1, &state.passBits.color2 };

	VkFormat colorAttachmentFormats[ 3 ] = {};
	for ( uint32_t i = 0; i < colorAttachmentCount; ++i ) {
		colorAttachmentFormats[ i ] = vk_GetTextureFormat( colorBits[ i ]->fmt );
	}

	VkPipelineRenderingCreateInfo renderingCreateInfo = {};
	renderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingCreateInfo.colorAttachmentCount    = colorAttachmentCount;
	renderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats;
	renderingCreateInfo.depthAttachmentFormat   = ( attachMask & RENDER_PASS_MASK_DEPTH   ) ? vk_GetTextureFormat( state.passBits.depth.fmt   ) : VK_FORMAT_UNDEFINED;
	renderingCreateInfo.stencilAttachmentFormat = ( attachMask & RENDER_PASS_MASK_STENCIL ) ? vk_GetTextureFormat( state.passBits.stencil.fmt ) : VK_FORMAT_UNDEFINED;

	pipelineInfo.pNext      = &renderingCreateInfo;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	pipelineInfo.pDepthStencilState = &depthStencil;

	VK_CHECK_RESULT( vkCreateGraphicsPipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipeline, VK_OBJECT_TYPE_PIPELINE,  ( "Pipeline (GFX): < " + vsBin.binName + " | " + psBin.binName + " >" ).c_str() );

	s_pipelineLib[ pipelineHdl.Get() ] = pipelineObject;

	s_progToPipelines[ state.progHdl.Get()].insert(state);

	return pipelineHdl;
}


void DestroyComputePipeline( const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state = {};
	state.progHdl = progAsset.Handle();

	const hdl_t pipelineHdl = GetComputePipelineStateHash( state );

	auto it = s_pipelineLib.find( pipelineHdl.Get() );
	if ( it == s_pipelineLib.end() ) {
		return;
	}
	vkDestroyPipeline( context.device, it->second.pipeline, nullptr );
	vkDestroyPipelineLayout( context.device, it->second.pipelineLayout, nullptr );

	s_pipelineLib.erase( it );
}


void CreateComputePipeline( const Asset<GpuProgram>& progAsset )
{
	const pipelineState_t state = CreateComputeState( progAsset );
	const hdl_t pipelineHdl = GetComputePipelineStateHash( state );
	return CreateComputePipeline( pipelineHdl, state );
}


void CreateComputePipeline( const hdl_t pipelineHdl, const pipelineState_t& state )
{
	auto it = s_pipelineLib.find( pipelineHdl.Get() );
	if ( it != s_pipelineLib.end() ) {
		return;
	}

	const GpuProgram& prog = *state.prog;

	VkDescriptorSetLayout layouts[ GpuProgram::MaxBindSets ];
	for ( uint32_t i = 0; i < prog.bindsetCount; ++i ) {
		layouts[ i ] = prog.bindsets[ i ]->GetVkObject();
	}

	pipelineObject_t pipelineObject;
	pipelineObject.state = state;
	pipelineObject.prog = state.prog;
	pipelineObject.dbgProgName = state.dbgProgName;

	const ShaderBin& csBin = prog.shaderBins[ 0 ].find( 0 )->second;
	assert( csBin.type == shaderType_t::COMPUTE );

	VkPipelineShaderStageCreateInfo computeShaderStageInfo {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = csBin.vk_shader;
	computeShaderStageInfo.pName = "CSMain";
	computeShaderStageInfo.pNext = nullptr;

	VkPushConstantRange pushRanges;
	pushRanges.offset = 0;
	pushRanges.size = 32;
	pushRanges.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.setLayoutCount = prog.bindsetCount;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushRanges;

	VK_CHECK_RESULT( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, ( "PipelineLayout (Compute): < " + csBin.binName + " >" ).c_str() );

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = 0;
	pipelineInfo.layout = pipelineObject.pipelineLayout;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.pNext = nullptr;

	VK_CHECK_RESULT( vkCreateComputePipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipeline, VK_OBJECT_TYPE_PIPELINE, ( "Pipeline (Compute): < " + csBin.binName + " >" ).c_str() );

	s_pipelineLib[ pipelineHdl.Get() ] = pipelineObject;

	s_progToPipelines[ state.progHdl.Get() ].insert( state );
}
