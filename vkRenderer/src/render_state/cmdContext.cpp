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
#include "cmdContext.h"
#include "deviceContext.h"
#include "../render_binding/pipeline.h"


VkCommandBuffer& CommandContext::CommandBuffer()
{
	return commandBuffers[ context.bufferId ];
}


void GfxContext::Create()
{
	// Pool creation
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create graphics command pool!" );
		}
	}

	// Buffer creation
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate graphics command buffers!" );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( commandBuffers[ i ], 0 );
		}
	}
}


void GfxContext::Destroy()
{
	vkFreeCommandBuffers( context.device, commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), commandBuffers );
	vkDestroyCommandPool( context.device, commandPool, nullptr );
}


void ComputeContext::Create()
{
	// Pool creation
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_COMPUTE ];
		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create compute command pool!" );
		}
	}

	// Buffer creation
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate compute command buffers!" );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( commandBuffers[ i ], 0 );
		}
	}
}


void ComputeContext::Destroy()
{
	vkFreeCommandBuffers( context.device, commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), commandBuffers );
	vkDestroyCommandPool( context.device, commandPool, nullptr );
}


void ComputeContext::Submit( const uint32_t bufferId )
{
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[ bufferId ];

	if ( vkQueueSubmit( context.computeContext, 1, &submitInfo, VK_NULL_HANDLE ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to submit compute command buffers!" );
	}
}


void ComputeContext::Dispatch( const hdl_t progHdl, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y, const uint32_t z )
{
	pipelineState_t state = {};
	state.progHdl = progHdl;

	const hdl_t pipelineHdl = Hash( reinterpret_cast<const uint8_t*>( &state ), sizeof( state ) );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipelineHdl, &pipelineObject );

	VkCommandBuffer cmdBuffer = commandBuffers[ context.bufferId ];

	if ( pipelineObject != nullptr )
	{
		VkDescriptorSet set[1] = { bindParms.GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipelineLayout, 0, 1, set, 0, 0 );

		vkCmdDispatch( cmdBuffer, x, y, z );
	}
}


void UploadContext::Create()
{
	// Pool creation
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create upload command pool!" );
		}
	}

	// Buffer creation
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate upload command buffers!" );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( commandBuffers[ i ], 0 );
		}
	}
}


void UploadContext::Destroy()
{
	vkFreeCommandBuffers( context.device, commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), commandBuffers );
	vkDestroyCommandPool( context.device, commandPool, nullptr );
}