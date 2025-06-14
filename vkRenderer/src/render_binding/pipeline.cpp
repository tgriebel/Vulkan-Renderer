/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "../../stdafx.h"

#include "pipeline.h"
#include "../render_core/renderer.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"
#include "../render_binding/bindings.h"
#include "shaderBinding.h"
#include <gfxcore/core/assetLib.h>
#include <gfxcore/scene/scene.h>

static std::unordered_map< uint64_t, pipelineObject_t > g_pipelineLib;

static const uint32_t MaxVertexAttribs = 6;
static std::array<VkVertexInputAttributeDescription, MaxVertexAttribs> GetVertexAttributeDescriptions()
{
	uint32_t attribId = 0;

	std::array<VkVertexInputAttributeDescription, MaxVertexAttribs> attributeDescriptions{ };
	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, pos );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, color );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, normal );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, tangent );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, bitangent );
	++attribId;

	attributeDescriptions[ attribId ].binding = 0;
	attributeDescriptions[ attribId ].location = attribId;
	attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[ attribId ].offset = offsetof( vsInput_t, texCoord );
	++attribId;

	assert( attribId == MaxVertexAttribs );

	return attributeDescriptions;
}


void CreateBindingLayout( ShaderBindSet& bindSet, VkDescriptorSetLayout& layout )
{
	const uint32_t bindingCount = bindSet.Count();
	if( bindingCount == 0 )
	{
		assert( 0 );
		return;
	}

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.resize( bindingCount );

	for( uint32_t i = 0; i < bindingCount; ++i )
	{
		const ShaderBinding* binding = bindSet.GetBinding(i);
		
		layoutBindings[i] = {};
		layoutBindings[i].binding = binding->GetSlot();
		layoutBindings[i].descriptorCount = binding->GetMaxDescriptorCount();
		layoutBindings[i].descriptorType = vk_GetDescriptorType( binding->GetType() );
		layoutBindings[i].pImmutableSamplers = nullptr;
		layoutBindings[i].stageFlags = vk_GetStageFlags( binding->GetBindFlags() );
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>( layoutBindings.size() );
	layoutInfo.pBindings = layoutBindings.data();

	VK_CHECK_RESULT( vkCreateDescriptorSetLayout( context.device, &layoutInfo, nullptr, &layout ) );
}


void ClearPipelineCache()
{
	g_pipelineLib.clear();
}


void DestroyPipelineCache()
{
	for ( auto it = g_pipelineLib.begin(); it != g_pipelineLib.end(); ++it )
	{
		vkDestroyPipeline( context.device, it->second.pipeline, nullptr );
		vkDestroyPipelineLayout( context.device, it->second.pipelineLayout, nullptr );
	}
	g_pipelineLib.clear();
}


bool GetPipelineObject( hdl_t hdl, pipelineObject_t** pipelineObject )
{
	auto it = g_pipelineLib.find( hdl.Get() );
	if ( it != g_pipelineLib.end() ) {
		*pipelineObject = &it->second;
		return true;
	}
	return false;
}


hdl_t FindPipelineObject( const DrawPass* pass, const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state = {};
	state.stateBits = pass->StateBits();
	state.samplingRate = pass->SampleRate();
	state.progHdl = progAsset.Handle();
	state.passBits = pass->GetFrameBuffer()->GetAttachmentBits();

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	auto it = g_pipelineLib.find( pipelineHdl.Get() );
	if ( it != g_pipelineLib.end() ) {
		return pipelineHdl;
	}
	return INVALID_HDL;
}


void DestroyGraphicsPipeline( const DrawPass* pass, const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state = {};
	state.stateBits = pass->StateBits();
	state.samplingRate = pass->SampleRate();
	state.progHdl = progAsset.Handle();
	state.passBits = pass->GetFrameBuffer()->GetAttachmentBits();

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	auto it = g_pipelineLib.find( pipelineHdl.Get() );
	if ( it == g_pipelineLib.end() ) {
		return;
	}
	vkDestroyPipeline( context.device, it->second.pipeline, nullptr );
	vkDestroyPipelineLayout( context.device, it->second.pipelineLayout, nullptr );

	g_pipelineLib.erase( it );
}


hdl_t CreateGraphicsPipeline( const RenderContext* renderContext, const DrawPass* pass, const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state = {};
	state.stateBits = pass->StateBits();
	state.samplingRate = pass->SampleRate();
	state.progHdl = progAsset.Handle();
	state.passBits = pass->GetFrameBuffer()->GetAttachmentBits();

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	auto it = g_pipelineLib.find( pipelineHdl.Get() );
	if ( it != g_pipelineLib.end() ) {
		return pipelineHdl;
	}

	const GpuProgram& prog = progAsset.Get();

	pipelineObject_t pipelineObject;
	pipelineObject.state = state;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{ };
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = prog.vk_shaders[ 0 ];
	vertShaderStageInfo.pName = "main";

	assert( prog.shaders[ 0].type == shaderType_t::VERTEX );

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{ };
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = prog.vk_shaders[ 1 ];
	fragShaderStageInfo.pName = "main";

	assert( prog.shaders[ 1 ].type == shaderType_t::PIXEL );

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescription{ };
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof( vsInput_t );
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	auto attributeDescriptions = GetVertexAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{ };
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size() );
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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

	for( uint32_t i = 0; i < colorAttachmentCount; ++i )
	{
		if( blendEnable )
		{
			colorBlendAttachments[ i ].colorWriteMask = colorFlags[ i ];
			colorBlendAttachments[ i ].blendEnable = VK_TRUE;
			colorBlendAttachments[ i ].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachments[ i ].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachments[ i ].colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachments[ i ].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachments[ i ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachments[ i ].alphaBlendOp = VK_BLEND_OP_ADD;
		}
		else
		{
			colorBlendAttachments[ i ].colorWriteMask = colorFlags[ i ];
			colorBlendAttachments[ i ].blendEnable = VK_FALSE;
			colorBlendAttachments[ i ].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachments[ i ].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachments[ i ].colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachments[ i ].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachments[ i ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachments[ i ].alphaBlendOp = VK_BLEND_OP_ADD;
		}
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

	VkPushConstantRange pushRanges;
	pushRanges.offset = 0;
	pushRanges.size = sizeof( pushConstants_t );
	pushRanges.stageFlags = VK_SHADER_STAGE_ALL;
	pipelineLayoutInfo.pPushConstantRanges = &pushRanges;

	VK_CHECK_RESULT( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) );

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
	pipelineInfo.stageCount = 2;
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
	pipelineInfo.renderPass = pass->GetFrameBuffer()->GetVkRenderPass();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	pipelineInfo.pDepthStencilState = &depthStencil;

	VK_CHECK_RESULT( vkCreateGraphicsPipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) );

	g_pipelineLib[ pipelineHdl.Get() ] = pipelineObject;

	return pipelineHdl;
}


void DestroyComputePipeline( const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state = {};
	state.progHdl = progAsset.Handle();

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	auto it = g_pipelineLib.find( pipelineHdl.Get() );
	if ( it == g_pipelineLib.end() ) {
		return;
	}
	vkDestroyPipeline( context.device, it->second.pipeline, nullptr );
	vkDestroyPipelineLayout( context.device, it->second.pipelineLayout, nullptr );

	g_pipelineLib.erase( it );
}


void CreateComputePipeline( const Asset<GpuProgram>& progAsset )
{
	pipelineState_t state = {};
	state.progHdl = progAsset.Handle();

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	auto it = g_pipelineLib.find( pipelineHdl.Get() );
	if ( it != g_pipelineLib.end() ) {
		return;
	}

	const GpuProgram& prog = progAsset.Get();

	VkDescriptorSetLayout layouts[ GpuProgram::MaxBindSets ];
	for ( uint32_t i = 0; i < prog.bindsetCount; ++i ) {
		layouts[ i ] = prog.bindsets[ i ]->GetVkObject();
	}

	pipelineObject_t pipelineObject;
	pipelineObject.state = state;

	VkPipelineShaderStageCreateInfo computeShaderStageInfo {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = prog.vk_shaders[ 0 ];
	computeShaderStageInfo.pName = "main";
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

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = 0;
	pipelineInfo.layout = pipelineObject.pipelineLayout;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.pNext = nullptr;

	pipelineObject.csName = prog.shaders->name.c_str();

	VK_CHECK_RESULT( vkCreateComputePipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) );

	g_pipelineLib[ pipelineHdl.Get() ] = pipelineObject;
}