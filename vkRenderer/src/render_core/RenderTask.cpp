#include "RenderTask.h"
#include "../render_state/cmdContext.h"
#include "../globals/renderview.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

void RenderTask::RenderViewSurfaces( CommandContext& cmdContext )
{
	const drawPass_t passBegin = renderView->ViewRegionPassBegin();
	const drawPass_t passEnd = renderView->ViewRegionPassEnd();

	// For now the pass state is the same for the entire view region
	const DrawPass* pass = renderView->passes[ passBegin ];
	if ( pass == nullptr ) {
		throw std::runtime_error( "Missing pass state!" );
	}

	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	VkBuffer vertexBuffers[] = { renderView->vb->GetVkObject() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers( cmdContext.CommandBuffer(), 0, 1, vertexBuffers, offsets );
	vkCmdBindIndexBuffer( cmdContext.CommandBuffer(), renderView->ib->GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

	for ( uint32_t passIx = passBegin; passIx <= passEnd; ++passIx )
	{
		const DrawPass* pass = renderView->passes[ passIx ];
		if ( pass == nullptr ) {
			continue;
		}

		cmdContext.MarkerBeginRegion( pass->name, ColorToVector( Color::White ) );

		// Pass Begin
		{
			VkRenderPassBeginInfo passInfo{ };
			passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passInfo.renderPass = pass->fb->GetVkRenderPass( pass->transitionState );
			passInfo.framebuffer = pass->fb->GetVkBuffer( pass->transitionState, pass->transitionState.flags.presentAfter ? context.swapChainIndex : context.bufferId );
			passInfo.renderArea.offset = { pass->viewport.x, pass->viewport.y };
			passInfo.renderArea.extent = { pass->viewport.width, pass->viewport.height };

			const VkClearColorValue clearColor = { pass->clearColor[ 0 ], pass->clearColor[ 1 ], pass->clearColor[ 2 ], pass->clearColor[ 3 ] };
			const VkClearDepthStencilValue clearDepth = { pass->clearDepth, pass->clearStencil };

			const uint32_t colorAttachmentsCount = pass->fb->GetColorLayers();
			const uint32_t attachmentsCount = pass->fb->GetLayers();

			passInfo.clearValueCount = 0;
			passInfo.pClearValues = nullptr;

			std::array<VkClearValue, 5> clearValues{ };
			assert( attachmentsCount <= 5 );

			if ( pass->transitionState.flags.clear )
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

			const viewport_t& viewport = renderView->GetViewport();

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
		}

		sortKey_t lastKey = {};
		lastKey.materialId = INVALID_HDL.Get();
		lastKey.stencilBit = 0;

		if ( passIx == drawPass_t::DRAWPASS_DEBUG_2D )
		{
#ifdef USE_IMGUI
			cmdContext.MarkerBeginRegion( "Debug Menus", ColorToVector( Color::White ) );

			// DrawDebugMenu();
			ImGui::Begin( "Control Panel" );
			ImGui::End();

			// Render dear imgui into screen
			ImGui::Render();
			ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );
			cmdContext.MarkerEndRegion();
#endif
			vkCmdEndRenderPass( cmdBuffer );
			continue;
		}

		for ( size_t surfIx = 0; surfIx < renderView->mergedModelCnt; surfIx++ )
		{
			drawSurf_t& surface = renderView->merged[ surfIx ];
			surfaceUpload_t& upload = renderView->uploads[ surfIx ];

			if ( SkipPass( surface, drawPass_t( passIx ) ) ) {
				continue;
			}

			pipelineObject_t* pipelineObject = nullptr;
			GetPipelineObject( surface.pipelineObject[ passIx ], &pipelineObject );
			if ( pipelineObject == nullptr ) {
				continue;
			}

			cmdContext.MarkerInsert( surface.dbgName, ColorToVector( Color::LGrey ) );

			if ( lastKey.key != surface.sortKey.key )
			{
				if ( passIx == DRAWPASS_DEPTH ) {
					// vkCmdSetDepthBias
					vkCmdSetStencilReference( cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
				}

				const uint32_t descSetCount = 1;
				VkDescriptorSet descSetArray[ descSetCount ] = { pass->parms[ context.bufferId ]->GetVkObject() };

				vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
				vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );
				lastKey = surface.sortKey;
			}

			assert( surface.sortKey.materialId < ( 1ull << KeyMaterialBits ) );

			pushConstants_t pushConstants = { surface.objectId, uint32_t( surface.sortKey.materialId ), uint32_t( renderView->GetViewId() ) };
			vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( cmdBuffer, upload.indexCount, renderView->instanceCounts[ surfIx ], upload.firstIndex, upload.vertexOffset, 0 );
		}

		vkCmdEndRenderPass( cmdBuffer );
		cmdContext.MarkerEndRegion();
	}
}


void RenderTask::Init( RenderView* view, drawPass_t begin, drawPass_t end )
{
	renderView = view;
	beginPass = begin;
	endPass= end;

	finishedSemaphore.Create( "Task Finished" );
}


void RenderTask::Shuntdown()
{
	finishedSemaphore.Destroy();
}


void RenderTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( renderView->GetName(), ColorToVector( Color::White ) );

	RenderViewSurfaces( context );

	context.MarkerEndRegion();
}


uint32_t RenderSchedule::PendingTasks() const
{
	return ( static_cast<uint32_t>( tasks.size() ) - currentTask );
}


void RenderSchedule::Reset()
{
	currentTask = 0;
}


void RenderSchedule::Clear()
{
	tasks.clear();
}


void RenderSchedule::Queue( GpuTask* task )
{
	tasks.push_back( task );
}


void RenderSchedule::IssueNext( CommandContext& context )
{
	GpuTask* task = tasks[ currentTask ];
	++currentTask;

	task->Execute( context );
}