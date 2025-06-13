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

#include "RenderTask.h"
#include "renderer.h"
#include "../render_state/cmdContext.h"
#include "../globals/renderview.h"
#include "../render_binding/gpuResources.h"
#include "../render_binding/bindings.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"

extern imguiControls_t			g_imguiControls;
#endif

static inline bool SkipPass( const drawSurf_t& surf, const drawPass_t pass )
{
	if ( surf.pipelineObject == INVALID_HDL ) {
		return true;
	}

	if ( ( surf.flags & SKIP_OPAQUE ) != 0 )
	{
		if ( ( pass == DRAWPASS_SHADOW ) ||
			( pass == DRAWPASS_DEPTH ) ||
			( pass == DRAWPASS_TERRAIN ) ||
			( pass == DRAWPASS_OPAQUE ) ||
			( pass == DRAWPASS_SKYBOX ) ||
			( pass == DRAWPASS_DEBUG_3D )
			) {
			return true;
		}
	}

	if ( ( pass == DRAWPASS_DEBUG_3D ) && ( ( surf.flags & DEBUG_SOLID ) == 0 ) ) {
		return true;
	}

	if ( ( pass == DRAWPASS_DEBUG_WIREFRAME ) && ( ( surf.flags & WIREFRAME ) == 0 ) ) {
		return true;
	}

	return false;
}


void RenderTask::RenderViewSurfaces( GfxContext* cmdContext )
{
	const drawPass_t passBegin = renderView->ViewRegionPassBegin();
	const drawPass_t passEnd = renderView->ViewRegionPassEnd();

	// For now the pass state is the same for the entire view region
	const DrawPass* pass = renderView->passes[ passBegin ];
	if ( pass == nullptr ) {
		throw std::runtime_error( "Missing pass state!" );
	}

	const renderPassTransition_t& transitionState = renderView->TransitionState();

	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = pass->GetFrameBuffer()->GetVkRenderPass( transitionState );
	passInfo.framebuffer = pass->GetFrameBuffer()->GetVkBuffer( transitionState, context.bufferId );
	passInfo.renderArea.offset = { pass->GetViewport().x, pass->GetViewport().y };
	passInfo.renderArea.extent = { pass->GetViewport().width, pass->GetViewport().height };

	const vec4f clearColor = renderView->ClearColor();
	const float clearDepth = renderView->ClearDepth();
	const uint32_t clearStencil = renderView->ClearStencil();

	const VkClearColorValue vk_clearColor = { clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] };
	const VkClearDepthStencilValue vk_clearDepth = { clearDepth, clearStencil };

	const uint32_t colorAttachmentsCount = pass->GetFrameBuffer()->ColorLayerCount();
	const uint32_t attachmentsCount = pass->GetFrameBuffer()->LayerCount();

	passInfo.clearValueCount = 0;
	passInfo.pClearValues = nullptr;

	std::array<VkClearValue, 5> clearValues{ };
	assert( attachmentsCount <= 5 );

	if ( transitionState.flags.clear )
	{
		for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
			clearValues[ i ].color = vk_clearColor;
		}

		for ( uint32_t i = colorAttachmentsCount; i < attachmentsCount; ++i ) {
			clearValues[ i ].depthStencil = vk_clearDepth;
		}

		passInfo.clearValueCount = attachmentsCount;
		passInfo.pClearValues = clearValues.data();
	}

	VkCommandBuffer cmdBuffer = cmdContext->CommandBuffer();

	for ( uint32_t passIx = passBegin; passIx <= passEnd; ++passIx )
	{
		DrawPass* pass = renderView->passes[ passIx ];
		if ( pass == nullptr ) {
			continue;
		}
		// These barriers could be tighter by places them in between passes
		pass->InsertResourceBarriers( *cmdContext );
	}

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	uint32_t modelOffset = 0;

	for ( uint32_t passIx = passBegin; passIx <= passEnd; ++passIx )
	{
		DrawPass* pass = renderView->passes[ passIx ];
		if ( pass == nullptr ) {
			continue;
		}

		// FIXME: Make this it's own task
		if ( passIx == drawPass_t::DRAWPASS_DEBUG_2D )
		{
#ifdef USE_IMGUI
			cmdContext->MarkerBeginRegion( "Debug Menus", ColorToVector( Color::White ) );
			ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );
			cmdContext->MarkerEndRegion();
#endif
			continue;
		}

		const DrawGroup* drawGroup = &renderView->drawGroup[ passIx ];

		const uint32_t surfaceCount = drawGroup->Count();
		if( surfaceCount == 0 ) {
			continue;
		}

		const GeometryContext* geo = drawGroup->Geometry();

		VkBuffer vertexBuffers[] = { geo->vb.GetVkObject() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers( cmdContext->CommandBuffer(), 0, 1, vertexBuffers, offsets );
		vkCmdBindIndexBuffer( cmdContext->CommandBuffer(), geo->ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

		cmdContext->MarkerBeginRegion( pass->Name(), ColorToVector( Color::White ) );

		const viewport_t& viewport = pass->GetViewport();

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

		sortKey_t lastKey = {};
		lastKey.materialId = INVALID_HDL.Get();
		lastKey.stencilBit = 0;

		hdl_t pipelineHandle = INVALID_HDL;
		pipelineObject_t* pipelineObject = nullptr;

		for ( uint32_t surfIx = 0; surfIx < surfaceCount; ++surfIx )
		{
			const drawSurf_t& surface = drawGroup->DrawSurf( surfIx );
			const surfaceUpload_t& upload = drawGroup->SurfUpload( surfIx );

			if ( SkipPass( surface, drawPass_t( passIx ) ) ) {
				continue;
			}

			if ( lastKey.key != surface.sortKey.key )
			{
				cmdContext->MarkerInsert( surface.dbgName, ColorToVector( Color::LGrey ) );

				if ( passIx == DRAWPASS_DEPTH ) {
					vkCmdSetStencilReference( cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
				}

				if( surface.pipelineObject != pipelineHandle )
				{
					GetPipelineObject( surface.pipelineObject, &pipelineObject );
					if ( pipelineObject == nullptr ) {
						continue;
					}

					const RenderContext* renderContext = cmdContext->GetRenderContext();

					const uint32_t descSetCount = 3;
					VkDescriptorSet descSetArray[ descSetCount ] = { renderContext->globalParms->GetVkObject(), renderView->BindParms()->GetVkObject(), pass->parms->GetVkObject() };

					vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );
					pipelineHandle = surface.pipelineObject;
				}
				lastKey = surface.sortKey;
			}

			assert( surface.sortKey.materialId < ( 1ull << KeyMaterialBits ) );

			pushConstants_t pushConstants = {};
			pushConstants.viewId = uint32_t( renderView->GetViewId() );
			pushConstants.objectId = surface.objectOffset + renderView->drawGroupOffset[ passIx ];
			pushConstants.materialId = uint32_t( surface.sortKey.materialId );

			vkCmdPushConstants( cmdBuffer, pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

			vkCmdDrawIndexed( cmdBuffer, upload.indexCount, drawGroup->InstanceCount( surfIx ), upload.firstIndex, upload.vertexOffset, 0 );
		}	
		cmdContext->MarkerEndRegion();
	}

	vkCmdEndRenderPass( cmdBuffer );
}


void RenderTask::Init( RenderView* view, drawPass_t begin, drawPass_t end )
{
	renderView = view;
	beginPass = begin;
	endPass= end;

	finishedSemaphore.Create( "Task Finished" );
}


void RenderTask::Shutdown()
{
	finishedSemaphore.Destroy();
}


void RenderTask::FrameBegin()
{
	if( renderView != nullptr ) {
		renderView->FrameBegin();
	}
}


void RenderTask::FrameEnd()
{
	if ( renderView != nullptr ) {
		renderView->FrameEnd();
	}
}


void RenderTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( renderView->GetName(), ColorToVector( Color::White ) );

	RenderViewSurfaces( reinterpret_cast<GfxContext*>( &context ) );

	context.MarkerEndRegion();
}


ComputeTask::ComputeTask( const char* csName, ComputeState* state )
{
	m_state = state;
	m_progHdl = g_assets.gpuPrograms.RetrieveHdl( csName );
}


void ComputeTask::FrameBegin()
{

}


void ComputeTask::FrameEnd()
{

}


void ComputeTask::Execute( CommandContext& context )
{
	ComputeContext* computeContext = reinterpret_cast<ComputeContext*>( &context );
	computeContext->Dispatch( m_progHdl, *m_state->parms, m_state->x, m_state->y, m_state->z );
}


void TransitionImageTask::Execute( CommandContext& context )
{
	Transition( &context, *m_img, m_srcState, m_dstState );
}


void CopyImageTask::Execute( CommandContext& context )
{
	CopyImage( &context, *m_src, *m_dst );
}


uint32_t RenderSchedule::PendingTasks() const
{
	return ( static_cast<uint32_t>( tasks.size() ) - currentTask );
}


void RenderSchedule::Clear()
{
	RenderResource::Cleanup( resourceLifeTime_t::TASK );
	for( size_t i = 0; i < tasks.size(); ++i ) {
		delete tasks[ i ];
	}
	tasks.clear();
}


void RenderSchedule::Queue( GpuTask* task )
{
	// FIXME: must own pointer
	assert( task );
	tasks.push_back( task );
}


void RenderSchedule::FrameBegin()
{
	currentTask = 0;

	const uint32_t taskCount = static_cast<uint32_t>( tasks.size() );
	for( uint32_t i = 0; i < taskCount; ++i )
	{
		GpuTask* task = tasks[ i ];
		task->FrameBegin();
	}

#ifdef USE_IMGUI
	// Prepare dear imgui render data
	ImGui::Render();
#endif
}


void RenderSchedule::FrameEnd()
{
	const uint32_t taskCount = static_cast<uint32_t>( tasks.size() );
	for ( uint32_t i = 0; i < taskCount; ++i )
	{
		GpuTask* task = tasks[ i ];
		task->FrameEnd();
	}
}


void RenderSchedule::IssueNext( CommandContext& context )
{
	GpuTask* task = tasks[ currentTask ];
	++currentTask;

	task->Execute( context );
}