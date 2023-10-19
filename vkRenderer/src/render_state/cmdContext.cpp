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
#include <gfxcore/scene/assetManager.h> // FIXME: shouldn't expose this everywhere

#include "cmdContext.h"
#include "deviceContext.h"
#include "../render_core/renderer.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/imageView.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/bufferObjects.h"
#include "../render_binding/bindings.h"
#include "../globals/postEffect.h"

extern AssetManager g_assets;

/////////////////////////////////////////////
// Base Command Context
/////////////////////////////////////////////
VkCommandBuffer& CommandContext::CommandBuffer()
{
	return commandBuffers[ context.bufferId ];
}


void CommandContext::Begin()
{
	vkResetCommandBuffer( CommandBuffer(), 0 );

	waitSemaphores.clear();
	signalSemaphores.clear();

	VkCommandBufferBeginInfo beginInfo{ };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	VK_CHECK_RESULT( vkBeginCommandBuffer( CommandBuffer(), &beginInfo ) );

	isOpen = true;
}


void CommandContext::End()
{
	VK_CHECK_RESULT( vkEndCommandBuffer( CommandBuffer() ) );
	isOpen = false;
}


void CommandContext::Create( const char* name, RenderContext* renderContext )
{
	m_renderContext = renderContext;

	// Pool creation
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ queueType ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT( vkCreateCommandPool( context.device, &poolInfo, nullptr, &commandPool ) );
		vk_MarkerSetObjectName( (uint64_t)commandPool, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, name );
	}

	// Buffer creation
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MaxFrameStates );

		VK_CHECK_RESULT( vkAllocateCommandBuffers( context.device, &allocInfo, commandBuffers ) );

		for ( size_t i = 0; i < MaxFrameStates; i++ )
		{
			vkResetCommandBuffer( commandBuffers[ i ], 0 );
			vk_MarkerSetObjectName( (uint64_t)commandBuffers[ i ], VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name );
		}
	}
}


void CommandContext::Destroy()
{
	vkFreeCommandBuffers( context.device, commandPool, static_cast<uint32_t>( MaxFrameStates ), commandBuffers );
	vkDestroyCommandPool( context.device, commandPool, nullptr );
}


void CommandContext::Wait( GpuSemaphore* semaphore )
{
	waitSemaphores.push_back( semaphore );
}


void CommandContext::Signal( GpuSemaphore* semaphore )
{
	signalSemaphores.push_back( semaphore );
}


void CommandContext::MarkerBeginRegion( const char* pMarkerName, const vec4f& color )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.color[ 0 ] = color[ 0 ];
		markerInfo.color[ 1 ] = color[ 1 ];
		markerInfo.color[ 2 ] = color[ 2 ];
		markerInfo.color[ 3 ] = color[ 3 ];
		markerInfo.pMarkerName = pMarkerName;
		context.fnCmdDebugMarkerBegin( CommandBuffer(), &markerInfo );
	}
}


void CommandContext::MarkerEndRegion()
{
	if ( context.debugMarkersEnabled && ( context.fnCmdDebugMarkerEnd != nullptr ) )
	{
		context.fnCmdDebugMarkerEnd( CommandBuffer() );
	}
}


void CommandContext::MarkerInsert( std::string markerName, const vec4f& color )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerMarkerInfoEXT markerInfo = {};
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy( markerInfo.color, &color[ 0 ], sizeof( float ) * 4 );
		markerInfo.pMarkerName = markerName.c_str();
		context.fnCmdDebugMarkerInsert( CommandBuffer(), &markerInfo );
	}
}


void CommandContext::Submit( const GpuFence* fence )
{
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::vector<VkPipelineStageFlags> vk_waitStages;
	std::vector<VkSemaphore> vk_waitSemaphores;	
	vk_waitStages.reserve( waitSemaphores.size() );
	vk_waitSemaphores.reserve( waitSemaphores.size() );

	for( GpuSemaphore* semaphore : waitSemaphores )
	{
		assert( semaphore->waitStage != VK_PIPELINE_STAGE_NONE_KHR );
		vk_waitStages.push_back( semaphore->waitStage );
		vk_waitSemaphores.push_back( semaphore->GetVkObject() );
	}

	std::vector<VkSemaphore> vk_signalSemaphores;
	vk_signalSemaphores.reserve( signalSemaphores.size() );

	for ( GpuSemaphore* semaphore : signalSemaphores ) {
		vk_signalSemaphores.push_back( semaphore->GetVkObject() );
	}

	if ( vk_waitSemaphores.size() > 0 )
	{
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>( vk_waitSemaphores.size() );
		submitInfo.pWaitSemaphores = vk_waitSemaphores.data();
		submitInfo.pWaitDstStageMask = vk_waitStages.data();
	}
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer();

	if( vk_signalSemaphores.size() > 0 )
	{
		submitInfo.signalSemaphoreCount = static_cast<uint32_t>( vk_signalSemaphores.size() );
		submitInfo.pSignalSemaphores = vk_signalSemaphores.data();
	}

	VkFence vk_fence = ( fence != nullptr ) ? fence->GetVkObject() : VK_NULL_HANDLE;

	VkQueue vk_queue = ( queueType == QUEUE_COMPUTE ) ? context.computeContext : context.gfxContext;

	VK_CHECK_RESULT( vkQueueSubmit( vk_queue, 1, &submitInfo, vk_fence ) );
}


/////////////////////////////////////////////
// Compute Context
/////////////////////////////////////////////
void ComputeContext::Submit()
{
	VkSubmitInfo submitInfo{ };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer();

	VK_CHECK_RESULT( vkQueueSubmit( context.computeContext, 1, &submitInfo, VK_NULL_HANDLE ) );
}


void ComputeContext::Dispatch( const hdl_t progHdl, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y, const uint32_t z )
{
	assert( isOpen );

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


void Transition( CommandContext* cmdCommand, Image& image, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	cmdCommand->MarkerBeginRegion( "Transition", ColorToVector( ColorWhite ) );

	vk_TransitionImageLayout( cmdCommand->CommandBuffer(), &image, current, next );

	cmdCommand->MarkerEndRegion();
}


void GenerateMipmaps( CommandContext* cmdCommand, Image& image )
{
	cmdCommand->MarkerBeginRegion( "GenerateMips", ColorToVector( ColorWhite ) );

	vk_GenerateMipmaps( cmdCommand->CommandBuffer(), &image );

	cmdCommand->MarkerEndRegion();
}


void CopyImage( CommandContext* cmdCommand, Image& src, Image& dst )
{
	cmdCommand->MarkerBeginRegion( "CopyImage", ColorToVector( ColorWhite ) );

	vk_CopyImage( cmdCommand->CommandBuffer(), &src, &dst );

	cmdCommand->MarkerEndRegion();
}


void CopyBufferToImage( CommandContext* cmdCommand, Image& texture, GpuBuffer& buffer, const uint64_t bufferOffset )
{
	cmdCommand->MarkerBeginRegion( "CopyBufferToImage", ColorToVector( ColorWhite ) );

	vk_CopyBufferToImage( cmdCommand->CommandBuffer(), &texture, buffer, bufferOffset );

	cmdCommand->MarkerEndRegion();
}


void GenerateDownsampleMips( CommandContext* cmdCommand, Image& image, downSampleMode_t mode )
{
	cmdCommand->MarkerBeginRegion( "GenerateDownsampleMips", ColorToVector( ColorWhite ) );

	std::vector<ImageView> views;
	views.resize( image.info.mipLevels );

	std::vector<DrawPass*> passes;
	passes.resize( image.info.mipLevels );

	std::vector<FrameBuffer> frameBuffers;
	frameBuffers.resize( image.info.mipLevels );

	for ( uint32_t i = 0; i < image.info.mipLevels; i++ )
	{
		imageSubResourceView_t subView = {};
		subView.baseMip = i;
		subView.mipLevels = 1;

		views[ i ].Init( image, image.info, subView );

		{
			frameBufferCreateInfo_t fbInfo = {};
			fbInfo.name = "TempDownsample";
			fbInfo.color0[ 0 ] = &views[ i ];
			fbInfo.width = views[ i ].info.width;
			fbInfo.height = views[ i ].info.height;
			fbInfo.lifetime = LIFETIME_TEMP;

			frameBuffers[ i ].Create( fbInfo );
		}
		passes[ i ] = new PostPass( &frameBuffers[ i ] );
	}

	vk_TransitionImageLayout( cmdCommand->CommandBuffer(), &views[ 0 ], GPU_IMAGE_NONE, GPU_IMAGE_READ );

	Asset<GpuProgram>* progAsset = g_assets.gpuPrograms.Find( AssetLibGpuProgram::Handle( "DownSample" ) );

	GpuBuffer buffer;
	//buffer.Create( "Resource buffer", LIFETIME_TEMP, 1, sizeof( imageProcessObject_t ), bufferType_t::UNIFORM, info.context->sharedMemory );

	for ( uint32_t i = 1; i < image.info.mipLevels; i++ )
	{
		vk_TransitionImageLayout( cmdCommand->CommandBuffer(), &views[ i ], GPU_IMAGE_NONE, GPU_IMAGE_TRANSFER_DST );

		//{
		//	const float w = float( passes[ i ]->GetFrameBuffer()->GetWidth() );
		//	const float h = float( passes[ i ]->GetFrameBuffer()->GetHeight() );

		//	imageProcessObject_t process = {};
		//	process.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );

		//	buffer.SetPos( 0 );
		//	buffer.CopyData( &process, sizeof( imageProcessObject_t ) );
		//}

		//passes[ i ]->codeImages.Resize( 3 );
		//passes[ i ]->codeImages[ 0 ] = &views[ i - 1 ];
		//passes[ i ]->codeImages[ 1 ] = &views[ i - 1 ];
		//passes[ i ]->codeImages[ 2 ] = &views[ i - 1 ];

		//passes[ i ]->parms->Bind( bind_globalsBuffer, &renderContext.globalConstants );
		//passes[ i ]->parms->Bind( bind_sourceImages, &passes[ i ]->codeImages );
		//passes[ i ]->parms->Bind( bind_imageStencil, &renderContext.stencilImageView );
		//passes[ i ]->parms->Bind( bind_imageProcess, &buffer );

		hdl_t pipeLineHandle = CreateGraphicsPipeline( cmdCommand->GetRenderContext(), passes[ i ], *progAsset );
		vk_RenderImageShader( *cmdCommand, pipeLineHandle, passes[ i ] );

		vk_TransitionImageLayout( cmdCommand->CommandBuffer(), &views[ i ], GPU_IMAGE_TRANSFER_DST, GPU_IMAGE_READ );
	}

	for ( uint32_t i = 0; i < image.info.mipLevels; i++ )
	{
		views[ i ].Destroy();
		if ( passes[ i ] != nullptr ) {
			delete passes[ i ];
		}
		frameBuffers[ i ].Destroy();
	}

	cmdCommand->MarkerEndRegion();
}