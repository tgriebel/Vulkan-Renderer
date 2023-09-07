#include "postEffect.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"

extern AssetManager g_assets;

void ImageProcess::Init( const imageProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "ImageProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	m_dbgName = info.name;

	pass = new DrawPass();

	pass->name = GetPassDebugName( DRAWPASS_POST_2D );
	pass->viewport.x = 0;
	pass->viewport.y = 0;
	pass->viewport.width = info.fb->GetWidth();
	pass->viewport.height = info.fb->GetHeight();
	pass->fb = info.fb;

	pass->stateBits = GFX_STATE_NONE;
	pass->passId = drawPass_t( DRAWPASS_POST_2D );

	m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

	pass->stateBits |= GFX_STATE_BLEND_ENABLE;
	pass->stateBits |= GFX_STATE_MSAA_ENABLE;
	pass->sampleRate = info.fb->GetColor()->info.subsamples;

	m_transitionState = {};
	m_transitionState.flags.clear = info.clear;
	m_transitionState.flags.store = true;
	m_transitionState.flags.presentAfter = info.present;
	m_transitionState.flags.readAfter = !info.present;

	m_progAsset = g_assets.gpuPrograms.Find( info.progHdl );

	buffer.Create( "Resource buffer", LIFETIME_TEMP, 1, sizeof( imageProcessObject_t ), bufferType_t::UNIFORM, info.context->sharedMemory );
}


void ImageProcess::Shutdown()
{
	buffer.Destroy();

	delete pass;
}


void ImageProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::White ) );

	{
		const float w = float( pass->fb->GetWidth() );
		const float h = float( pass->fb->GetHeight() );

		imageProcessObject_t process = {};
		process.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );

		buffer.SetPos( 0 );
		buffer.CopyData( &process, sizeof( imageProcessObject_t ) );
	}

	hdl_t pipeLineHandle = CreateGraphicsPipeline( pass, *m_progAsset );

	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = pass->fb->GetVkRenderPass( m_transitionState );
	passInfo.framebuffer = pass->fb->GetVkBuffer( m_transitionState, m_transitionState.flags.presentAfter ? context.swapChainIndex : context.bufferId );
	passInfo.renderArea.offset = { pass->viewport.x, pass->viewport.y };
	passInfo.renderArea.extent = { pass->viewport.width, pass->viewport.height };

	const VkClearColorValue clearColor = { m_clearColor[ 0 ], m_clearColor[ 1 ], m_clearColor[ 2 ], m_clearColor[ 3 ] };
	const VkClearDepthStencilValue clearDepth = { m_clearDepth, m_clearStencil };

	const uint32_t colorAttachmentsCount = pass->fb->GetColorLayers();
	const uint32_t attachmentsCount = pass->fb->GetLayers();

	passInfo.clearValueCount = 0;
	passInfo.pClearValues = nullptr;

	std::array<VkClearValue, 5> clearValues{ };
	assert( attachmentsCount <= 5 );

	if ( m_transitionState.flags.clear )
	{
		for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
			clearValues[ i ].color = clearColor;
		}

		for ( uint32_t i = colorAttachmentsCount; i < attachmentsCount; ++i ) {
			clearValues[ i ].depthStencil = clearDepth;
		}

		passInfo.clearValueCount = attachmentsCount;
		passInfo.pClearValues = clearValues.data();
	}

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	const viewport_t& viewport = pass->viewport;

	VkViewport vk_viewport{ };
	vk_viewport.x = static_cast<float>( viewport.x );
	vk_viewport.y = static_cast<float>( viewport.y );
	vk_viewport.width = static_cast<float>( viewport.width );
	vk_viewport.height = static_cast<float>( viewport.height );
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

	VkRect2D rect{ };
	rect.extent.width = viewport.width;
	rect.extent.height = viewport.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipeLineHandle, &pipelineObject );
	if ( pipelineObject != nullptr ) {
		const uint32_t descSetCount = 1;
		VkDescriptorSet descSetArray[ descSetCount ] = { pass->parms->GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );

		vkCmdDraw( cmdBuffer, 3, 1, 0, 0 );
	}

	vkCmdEndRenderPass( cmdBuffer );

	cmdContext.MarkerEndRegion();
}