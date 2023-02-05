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
#include "../render_state/deviceContext.h"
#include <core/assetLib.h>
#include <scene/scene.h>

AssetLib< pipelineObject_t > pipelineLib;

static VkVertexInputBindingDescription GetVertexBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription{ };
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof( vsInput_t );
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

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

void CreateSceneRenderDescriptorSetLayout( VkDescriptorSetLayout& layout )
{
	VkDescriptorSetLayoutBinding globalsLayoutBinding{ };
	globalsLayoutBinding.binding = 0;
	globalsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	globalsLayoutBinding.descriptorCount = 1;
	globalsLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
	globalsLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding viewLayoutBinding{ };
	viewLayoutBinding.binding = 1;
	viewLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	viewLayoutBinding.descriptorCount = 1;
	viewLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
	viewLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding uboLayoutBinding{ };
	uboLayoutBinding.binding = 2;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding sampler2dLayoutBinding{ };
	sampler2dLayoutBinding.binding = 3;
	sampler2dLayoutBinding.descriptorCount = MaxImageDescriptors;
	sampler2dLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler2dLayoutBinding.pImmutableSamplers = nullptr;
	sampler2dLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

	VkDescriptorSetLayoutBinding samplerCubeLayoutBinding{ };
	samplerCubeLayoutBinding.binding = 4;
	samplerCubeLayoutBinding.descriptorCount = MaxImageDescriptors;
	samplerCubeLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerCubeLayoutBinding.pImmutableSamplers = nullptr;
	samplerCubeLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

	VkDescriptorSetLayoutBinding materialBinding{ };
	materialBinding.binding = 5;
	materialBinding.descriptorCount = 1;
	materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	materialBinding.pImmutableSamplers = nullptr;
	materialBinding.stageFlags = VK_SHADER_STAGE_ALL;

	VkDescriptorSetLayoutBinding lightBinding{ };
	lightBinding.binding = 6;
	lightBinding.descriptorCount = 1;
	lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightBinding.pImmutableSamplers = nullptr;
	lightBinding.stageFlags = VK_SHADER_STAGE_ALL;

	VkDescriptorSetLayoutBinding codeImageBinding{ };
	codeImageBinding.binding = 7;
	codeImageBinding.descriptorCount = MaxCodeImages;
	codeImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	codeImageBinding.pImmutableSamplers = nullptr;
	codeImageBinding.stageFlags = VK_SHADER_STAGE_ALL;

	VkDescriptorSetLayoutBinding stencilBinding{ };
	stencilBinding.binding = 8;
	stencilBinding.descriptorCount = 1;
	stencilBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	stencilBinding.pImmutableSamplers = nullptr;
	stencilBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 9> bindings = {	globalsLayoutBinding,
																viewLayoutBinding,
																uboLayoutBinding,
																sampler2dLayoutBinding,
																samplerCubeLayoutBinding,
																materialBinding,
																lightBinding,
																codeImageBinding,
																stencilBinding };
	VkDescriptorSetLayoutCreateInfo layoutInfo{ };
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>( bindings.size() );
	layoutInfo.pBindings = bindings.data();

	if ( vkCreateDescriptorSetLayout( context.device, &layoutInfo, nullptr, &layout ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to create descriptor set layout!" );
	}
}

/*
void CreateDescriptorSets( GpuProgram& program )
{
}
*/

bool GetPipelineObject( hdl_t hdl, pipelineObject_t** pipelineObject )
{
	Asset<pipelineObject_t>* object = pipelineLib.Find( hdl );
	if( object != nullptr )
	{
		*pipelineObject = &object->Get();
		return true;
	}
	return false;
}


void CreateGraphicsPipeline( VkDescriptorSetLayout layout, VkRenderPass pass, const pipelineState_t& state, hdl_t& pipelineHdl )
{
	hdl_t assetHdl = pipelineLib.RetrieveHdl( state.tag );
	if ( assetHdl.IsValid() ) {
		pipelineHdl = assetHdl;
		return;
	}

	pipelineHdl = pipelineLib.Add( state.tag, pipelineObject_t() );

	pipelineObject_t& pipelineObject = pipelineLib.Find( pipelineHdl )->Get();
	pipelineObject.state = state;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{ };
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = state.shaders->vk_shaders[ 0 ];
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{ };
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = state.shaders->vk_shaders[ 1 ];
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	auto bindingDescription = GetVertexBindingDescription();
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
	viewport.x = state.viewport.x;
	viewport.y = state.viewport.y;
	viewport.width = state.viewport.width;
	viewport.height = state.viewport.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{ };
	scissor.offset = { static_cast<int32_t>( state.viewport.x ), static_cast<int32_t>( state.viewport.y ) };
	scissor.extent = { static_cast<uint32_t>( state.viewport.width ), static_cast<uint32_t>( state.viewport.height ) };

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
	if ( state.stateBits & GFX_STATE_MSAA_ENABLE ) {
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.rasterizationSamples = (VkSampleCountFlagBits)state.samplingRate;
	} else {
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	}
	multisampling.minSampleShading = 0.25f;
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkColorComponentFlags colorFlags = ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT );
	if ( state.stateBits & GFX_STATE_COLOR_MASK ) {
		colorFlags = 0;
	}

	const bool blendEnable = ( ( state.stateBits & GFX_STATE_BLEND_ENABLE ) != 0 );

	VkPipelineColorBlendAttachmentState colorBlendAttachment{ };
	if( blendEnable )
	{
		colorBlendAttachment.colorWriteMask = colorFlags;
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	else
	{
		colorBlendAttachment.colorWriteMask = colorFlags;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{ };
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[ 0 ] = 0.0f; // Optional
	colorBlending.blendConstants[ 1 ] = 0.0f; // Optional
	colorBlending.blendConstants[ 2 ] = 0.0f; // Optional
	colorBlending.blendConstants[ 3 ] = 0.0f; // Optional

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState{ };
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 3;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;

	VkDescriptorSetLayout layouts[] = { layout };
	pipelineLayoutInfo.pSetLayouts = layouts;
	
	pipelineLayoutInfo.pushConstantRangeCount = 1;

	VkPushConstantRange pushRanges;
	pushRanges.offset = 0;
	pushRanges.size = sizeof( pushConstants_t );
	pushRanges.stageFlags = VK_SHADER_STAGE_ALL;
	pipelineLayoutInfo.pPushConstantRanges = &pushRanges;

	if ( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) != VK_SUCCESS )	{
		throw std::runtime_error( "Failed to create pipeline layout!" );
	}

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
	pipelineInfo.renderPass = pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	pipelineInfo.pDepthStencilState = &depthStencil;

	if ( vkCreateGraphicsPipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create graphics pipeline!" );
	}
}

void CreateComputePipeline( VkDescriptorSetLayout layout, const pipelineState_t& state, hdl_t& pipelineHdl )
{
	pipelineHdl = pipelineLib.Add( state.tag, pipelineObject_t() );

	pipelineObject_t& pipelineObject = pipelineLib.Find( pipelineHdl )->Get();
	pipelineObject.state = state;

	VkPipelineShaderStageCreateInfo computeShaderStageInfo {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = state.shaders->vk_shaders[ 0 ];
	computeShaderStageInfo.pName = "main";
	computeShaderStageInfo.pNext = nullptr;

	// TODO: think about details
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	{
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;

		VkDescriptorSetLayout layouts[] = { layout };
		pipelineLayoutInfo.pSetLayouts = layouts;

		pipelineLayoutInfo.pushConstantRangeCount = 1;

		VkPushConstantRange pushRanges;
		pushRanges.offset = 0;
		pushRanges.size = sizeof( pushConstants_t );
		pushRanges.stageFlags = VK_SHADER_STAGE_ALL;
		pipelineLayoutInfo.pPushConstantRanges = &pushRanges;
	}
	//

	if ( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create pipeline layout!" );
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = 0;
	pipelineInfo.layout = pipelineObject.pipelineLayout;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.pNext = nullptr;

	if ( vkCreateComputePipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create compute pipeline!" );
	}
}