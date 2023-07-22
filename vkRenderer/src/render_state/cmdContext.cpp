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

void Fence::Create()
{
	VkFenceCreateInfo fenceInfo{ };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		if( vkCreateFence( context.device, &fenceInfo, nullptr, &fence[ i ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create synchronization objects for a frame!" );
		}
	}
}


void Fence::Destroy()
{
	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		vkDestroyFence( context.device, fence[ i ], nullptr );
	}
}


void Fence::Reset()
{
//	vkResetFences( context.device, 1, &fence[ m_frameId ] );
}


VkFence& Fence::VkObject()
{
	return fence[ context.bufferId ];
}


VkCommandBuffer& CommandContext::CommandBuffer()
{
	return commandBuffers[ context.bufferId ];
}


void CommandContext::Begin()
{
	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if ( vkBeginCommandBuffer( CommandBuffer(), &beginInfo ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to begin recording command buffer!" );
	}
}


void CommandContext::End()
{
	if ( vkEndCommandBuffer( CommandBuffer() ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to record command buffer!" );
	}
}


void CommandContext::Reset()
{
	vkResetCommandBuffer( CommandBuffer(), 0 );
}


void CommandContext::Create()
{
	// Pool creation
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ queueType ];
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
		allocInfo.commandBufferCount = static_cast<uint32_t>( MaxFrameStates );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate graphics command buffers!" );
		}

		for ( size_t i = 0; i < MaxFrameStates; i++ ) {
			vkResetCommandBuffer( commandBuffers[ i ], 0 );
		}
	}
}


void CommandContext::Destroy()
{
	vkFreeCommandBuffers( context.device, commandPool, static_cast<uint32_t>( MaxFrameStates ), commandBuffers );
	vkDestroyCommandPool( context.device, commandPool, nullptr );
}


void ComputeContext::Submit()
{
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer();

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

	VkCommandBuffer cmdBuffer = CommandBuffer();

	if ( pipelineObject != nullptr )
	{
		VkDescriptorSet set[1] = { bindParms.GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineObject->pipelineLayout, 0, 1, set, 0, 0 );

		vkCmdDispatch( cmdBuffer, x, y, z );
	}
}


void UploadContext::Submit()
{
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer();

	vkQueueSubmit( context.gfxContext, 1, &submitInfo, VK_NULL_HANDLE );
}