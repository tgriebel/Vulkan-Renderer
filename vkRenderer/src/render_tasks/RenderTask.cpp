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
#include "../render_core/renderer.h"
#include "../render_state/cmdContext.h"
#include "../render_core/renderview.h"
#include "../render_binding/gpuResources.h"
#include "../render_binding/bindings.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

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


void RenderTask::RenderViewSurfaces( GfxContext* cmdContext, const uint32_t multiViewIndex )
{
	// For now the pass state is the same for the entire view region
	const DrawPass* pass = m_renderView->passes[ multiViewIndex ][ m_beginPass ];
	if ( pass == nullptr ) {
		throw std::runtime_error( "Missing pass state!" );
	}

	const FrameBuffer* fb = pass->GetFrameBuffer();

	const bool startOnFirstPass = ( m_beginPass == m_renderView->ViewRegionPassBegin() );
	const bool endOnLastPass = ( m_endPass == m_renderView->ViewRegionPassEnd() );
	const bool isBackBuffer = fb->IsBackbuffer();

	renderPassTransition_t transitionState = m_renderView->TransitionState();

	transitionState.flags.clear = m_renderView->Clear() && startOnFirstPass;
	transitionState.flags.store = true;

	// If this is the first write to the backbuffer then it needs to transition from the present state
	if( transitionState.flags.clear ) {
		transitionState.flags.presentBefore = isBackBuffer;
	}
	if( endOnLastPass && m_renderView->Finalize() ) {
		transitionState.flags.presentAfter = isBackBuffer;
	}

	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = fb->GetVkRenderPass( transitionState );
	passInfo.framebuffer = fb->GetVkBuffer( transitionState, context.bufferId );
	passInfo.renderArea.offset = { pass->GetViewport().x, pass->GetViewport().y };
	passInfo.renderArea.extent = { pass->GetViewport().width, pass->GetViewport().height };

	const uint32_t colorAttachmentsCount = fb->ColorLayerCount();
	const uint32_t attachmentsCount = fb->LayerCount();

	passInfo.clearValueCount = 0;
	passInfo.pClearValues = nullptr;

	std::array<VkClearValue, 5> clearValues{ };
	assert( attachmentsCount <= 5 );

	if ( transitionState.flags.clear )
	{
		const vec4f clearColor = m_renderView->ClearColor();
		const float clearDepth = m_renderView->ClearDepth();
		const uint32_t clearStencil = m_renderView->ClearStencil();

		const VkClearColorValue vk_clearColor = { clearColor[ 0 ], clearColor[ 1 ], clearColor[ 2 ], clearColor[ 3 ] };
		const VkClearDepthStencilValue vk_clearDepth = { clearDepth, clearStencil };

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

	for ( uint32_t passIx = m_beginPass; passIx <= m_endPass; ++passIx )
	{
		DrawPass* pass = m_renderView->passes[ multiViewIndex ][ passIx ];
		if ( pass == nullptr ) {
			continue;
		}
		// These barriers could be tighter by places them in between passes
		pass->InsertResourceBarriers( *cmdContext );
	}

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	uint32_t modelOffset = 0;

	for ( uint32_t passIx = m_beginPass; passIx <= m_endPass; ++passIx )
	{
		DrawPass* pass = m_renderView->passes[ multiViewIndex ][ passIx ];
		if ( pass == nullptr ) {
			continue;
		}

		const DrawGroup* drawGroup = &m_renderView->drawGroup[ passIx ];

		const uint32_t surfaceCount = drawGroup->Count();
		if( surfaceCount == 0 ) {
			continue;
		}

		const GeometryContext* geo = drawGroup->Geometry();

		cmdContext->MarkerBeginRegion( pass->Name(), ColorToVector( Color::Gold ) );

		VkBuffer vertexBuffers[] = { geo->vb.GetVkObject() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers( cmdContext->CommandBuffer(), 0, 1, vertexBuffers, offsets );
		vkCmdBindIndexBuffer( cmdContext->CommandBuffer(), geo->ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

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
					VkDescriptorSet descSetArray[ descSetCount ] = { renderContext->globalParms->GetVkObject(), m_renderView->BindParms()->GetVkObject(), pass->parms->GetVkObject() };

					vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );
					pipelineHandle = surface.pipelineObject;
				}
				lastKey = surface.sortKey;
			}

			assert( surface.sortKey.materialId < ( 1ull << KeyMaterialBits ) );

			pushConstants_t pushConstants = {};
			pushConstants.viewId = uint32_t( m_renderView->GetViewBufferId( multiViewIndex ) );
			pushConstants.objectId = surface.objectOffset + m_renderView->drawGroupOffset[ passIx ];
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
	m_renderView = view;
	m_beginPass = begin;
	m_endPass= end;

	m_finishedSemaphore.Create( "Task Finished" );
}


void RenderTask::Shutdown()
{
	m_finishedSemaphore.Destroy();
}


void RenderTask::FrameBegin()
{
	if( m_renderView != nullptr ) {
		m_renderView->FrameBegin( m_beginPass, m_endPass );
	}
}


void RenderTask::FrameEnd()
{
	if ( m_renderView != nullptr ) {
		m_renderView->FrameEnd( m_beginPass, m_endPass );
	}
}


void RenderTask::Resize()
{
	if ( m_renderView != nullptr ) {
		m_renderView->Resize();
	}
}


std::string RenderTask::AsString() const
{
	std::stringstream ss;
	ss << "<RenderTask: " << m_renderView->GetName() << ">";
	return ss.str();
}


void RenderTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( m_renderView->GetName(), ColorToVector( Color::Cyan ) );

	const uint32_t multiViewCount = m_renderView->GetMultiViewCount();
	for( uint32_t multiViewIndex = 0; multiViewIndex < multiViewCount; ++multiViewIndex ) {
		RenderViewSurfaces( reinterpret_cast<GfxContext*>( &context ), multiViewIndex );
	}

	context.MarkerEndRegion();
}


ComputeTask::ComputeTask( const char* csName, ComputeState* state )
{
	m_name = csName;
	m_state = state;
	m_progHdl = g_assets.gpuPrograms.RetrieveHdl( csName );
}


void ComputeTask::FrameBegin()
{

}


void ComputeTask::FrameEnd()
{

}


std::string ComputeTask::AsString() const
{
	std::stringstream ss;
	ss << "<ComputeTask: " << m_name << ">";
	return ss.str();
}


void ComputeTask::Execute( CommandContext& context )
{
	ComputeContext* computeContext = reinterpret_cast<ComputeContext*>( &context );
	computeContext->Dispatch( m_progHdl, *m_state->parms, m_state->x, m_state->y, m_state->z );
}


std::string TransitionImageTask::AsString() const
{
	std::stringstream ss;
	ss << "<TransitionImageTask: " << m_img->gpuImage->GetDebugName() << ">";
	return ss.str();
}


void TransitionImageTask::Execute( CommandContext& context )
{
	Transition( &context, *m_img, m_srcState, m_dstState );
}


std::string CopyImageTask::AsString() const
{
	std::stringstream ss;
	ss << "<CopyImageTask: " << "" << ">";
	return ss.str();
}


void CopyImageTask::SetSourceParms( const copyImageParms_t& src )
{
	m_srcParms = src;
}


void CopyImageTask::SetDestinationParms( const copyImageParms_t& dst )
{
	m_dstParms = dst;
}


void CopyImageTask::Execute( CommandContext& context )
{
	CopyImage( &context, *m_src, m_srcParms, *m_dst, m_dstParms );
}


uint32_t RenderSchedule::TaskCount() const
{
	return taskCount;
}


bool RenderSchedule::HasPendingTasks() const
{
	return ( currentTask != nullptr );
}


void RenderSchedule::Clear()
{
	RenderResource::Cleanup( resourceLifeTime_t::TASK );
	
	GpuTask* t = tasks;
	while( t != nullptr )
	{
		GpuTask* next = t->GetChild();
		delete t;
		t = next;
	}
	tasks = nullptr;
}


void RenderSchedule::Link( GpuTask* task )
{
	// FIXME: must own pointer
	assert( task );
	if( end == nullptr )
	{
		assert( tasks == nullptr );
		tasks = task;
		end = task;
	}
	else
	{
		end->SetChild( task );
		end = task;
	}

	++taskCount;
}


void RenderSchedule::FrameBegin()
{
	currentTask = tasks;

	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->FrameBegin();
		t = t->GetChild();
	}
}


void RenderSchedule::FrameEnd()
{
	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->FrameEnd();
		t = t->GetChild();
	}
	assert( currentTask == nullptr );
}


void RenderSchedule::Resize()
{
	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		t->Resize();
		t = t->GetChild();
	}
}


void RenderSchedule::IssueNext( CommandContext& context )
{
	currentTask->Execute( context );
	currentTask = currentTask->GetChild();
}


void RenderSchedule::AsString() const
{
	std::cout << "Schedule\n";

	GpuTask* t = tasks;
	while ( t != nullptr )
	{
		std::cout << "+ " << t->AsString() << "\n";
		t = t->GetChild();
	}
	std::cout << std::flush;
}