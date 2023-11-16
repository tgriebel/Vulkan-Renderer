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

#include "../../window.h"
#include "../../input.h"
#include "debugMenu.h"

extern Scene*		g_scene;
extern Window		g_window;

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


static void DrawDebugMenu( RenderView& view )
{
#if defined( USE_IMGUI )
	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "Open Scene", "CTRL+O" ) ) {
				g_imguiControls.openSceneFileDialog = true;
			}
			if ( ImGui::MenuItem( "Reload", "CTRL+R" ) ) {
				g_imguiControls.reloadScene = true;
			}
			if ( ImGui::MenuItem( "Import Obj", "CTRL+I" ) ) {
				g_imguiControls.openModelImportFileDialog = true;
			}
			ImGui::EndMenu();
		}
		if ( ImGui::BeginMenu( "Edit" ) )
		{
			if ( ImGui::MenuItem( "Undo", "CTRL+Z" ) ) {}
			if ( ImGui::MenuItem( "Redo", "CTRL+Y", false, false ) ) {}  // Disabled item
			ImGui::Separator();
			if ( ImGui::MenuItem( "Cut", "CTRL+X" ) ) {}
			if ( ImGui::MenuItem( "Copy", "CTRL+C" ) ) {}
			if ( ImGui::MenuItem( "Paste", "CTRL+V" ) ) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::Begin( "Control Panel" );

	if ( ImGui::BeginTabBar( "Tabs" ) )
	{
		for ( uint32_t i = 0; i < view.debugMenus.Count(); ++i ) {
			( *view.debugMenus[ i ] )( );
		}
		ImGui::EndTabBar();
	}

	ImGui::Separator();

	ImGui::InputInt( "Image Id", &g_imguiControls.dbgImageId );
	g_imguiControls.dbgImageId = Clamp( g_imguiControls.dbgImageId, -1, int( g_assets.textureLib.Count() - 1 ) );

	char entityName[ 256 ];
	if ( g_imguiControls.selectedEntityId >= 0 ) {
		sprintf_s( entityName, "%i: %s", g_imguiControls.selectedEntityId, g_assets.modelLib.FindName( g_scene->entities[ g_imguiControls.selectedEntityId ]->modelHdl ) );
	}
	else {
		memset( &entityName[ 0 ], 0, 256 );
	}

	ImGui::Text( "Mouse: (%f, %f)", (float)g_window.input.GetMouse().x, (float)g_window.input.GetMouse().y );
	ImGui::Text( "Mouse Dt: (%f, %f)", (float)g_window.input.GetMouse().dx, (float)g_window.input.GetMouse().dy );
	const vec4f cameraOrigin = g_scene->camera.GetOrigin();
	ImGui::Text( "Camera: (%f, %f, %f)", cameraOrigin[ 0 ], cameraOrigin[ 1 ], cameraOrigin[ 2 ] );

	const vec2f ndc = g_window.GetNdc( g_window.input.GetMouse().x, g_window.input.GetMouse().y );

	ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );
	ImGui::Text( "Frame Number: %d", g_renderDebugData.frameNumber );
	ImGui::SameLine();
	ImGui::Text( "FPS: %f", 1000.0f / g_renderDebugData.frameTimeMs );

	ImGui::End();
#endif
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
	passInfo.framebuffer = pass->GetFrameBuffer()->GetVkBuffer( transitionState, transitionState.flags.presentAfter ? context.swapChainIndex : context.bufferId );
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

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	uint32_t modelOffset = 0;

	for ( uint32_t passIx = passBegin; passIx <= passEnd; ++passIx )
	{
		DrawPass* pass = renderView->passes[ passIx ];
		if ( pass == nullptr ) {
			continue;
		}

		const DrawGroup* drawGroup = &renderView->drawGroup[ passIx ];

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

		if ( passIx == drawPass_t::DRAWPASS_DEBUG_2D )
		{
#ifdef USE_IMGUI
			cmdContext->MarkerBeginRegion( "Debug Menus", ColorToVector( Color::White ) );

			DrawDebugMenu( *renderView );
			ImGui::Begin( "Control Panel" );
			ImGui::End();

			// Render dear imgui into screen
			ImGui::Render();
			ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );
			cmdContext->MarkerEndRegion();
#endif
			cmdContext->MarkerEndRegion();
			continue;
		}

		const uint32_t surfaceCount = drawGroup->Count();

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
					if ( lastKey.stencilBit != surface.sortKey.stencilBit ) {
						vkCmdSetStencilReference( cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
					}
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


void MipImageTask::Init( const mipProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "MipImageTaskInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	m_dbgName = info.name;
	m_image = info.img;
	m_mode = info.mode;
	m_context = info.context;
	m_resources = info.resources;

	const uint32_t mipLevels = m_image->info.mipLevels;

	m_passes.resize( mipLevels );
	m_views.resize( mipLevels );
	m_frameBuffers.resize( mipLevels );
	m_bufferViews.resize( mipLevels );

	{
		m_tempImage.info = m_image->info;
		m_tempImage.info.mipLevels = 1;
		MipDimensions( 1, m_image->info.width, m_image->info.height, &m_tempImage.info.width, &m_tempImage.info.height );

		m_tempImage.gpuImage = new GpuImage();
		m_tempImage.gpuImage->Create( "tempMipImage", m_tempImage.info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, m_context->localMemory );
	}

	// Create buffer
	m_buffer.Create( "Resource buffer", LIFETIME_TEMP, mipLevels, sizeof( imageProcessObject_t ), bufferType_t::UNIFORM, m_context->sharedMemory );

	// The last view is only needed to create a frame buffer 
	for ( uint32_t i = 0; i < m_image->info.mipLevels; ++i )
	{
		imageSubResourceView_t subView = {};
		subView.baseMip = i;
		subView.mipLevels = 1;
		subView.baseArray = 0;
		subView.arrayCount = 1;

		m_views[ i ].Init( *m_image, m_image->info, subView );
	}
	
	// All but the first image need a framebuffer since they are being written to
	for ( uint32_t i = 1; i < mipLevels; ++i )
	{
		frameBufferCreateInfo_t info{};
		info.name = "MipDownsample";
		info.color0[ 0 ] = &m_tempImage;
		info.width = m_views[ i ].info.width;
		info.height = m_views[ i ].info.height;
		info.lifetime = resourceLifetime_t::LIFETIME_TEMP;

		m_frameBuffers[ i ].Create( info );

		imageProcessObject_t imageProcessParms{};

		const float w = float( m_views[ i - 1 ].info.width );
		const float h = float( m_views[ i - 1 ].info.height );
		imageProcessParms.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );

		m_bufferViews[ i ] = m_buffer.GetView( i, 1 );

		m_bufferViews[ i ].SetPos( 0 );
		m_bufferViews[ i ].CopyData( &imageProcessParms, sizeof( imageProcessObject_t ) );

		m_passes[ i ] = new PostPass( &m_frameBuffers[ i ] );
		//m_passes[ i ]->SetViewport( 0, 0, m_views[ i ].info.width, m_views[ i ].info.height );
		m_passes[ i ]->codeImages.Resize( 1 );
		m_passes[ i ]->codeImages[ 0 ] = &m_views[ i - 1 ];

		m_passes[ i ]->parms = m_context->RegisterBindParm( bindset_imageProcess );
	}
	m_firstFrame = true;
}


void MipImageTask::FrameBegin()
{
	for ( uint32_t i = 1; i < m_image->info.mipLevels; ++i )
	{
		m_passes[ i ]->parms->Bind( bind_sourceImages, &m_passes[ i ]->codeImages );
		m_passes[ i ]->parms->Bind( bind_imageStencil, &m_resources->stencilImageView );
		m_passes[ i ]->parms->Bind( bind_imageProcess, &m_bufferViews[ i ] );
	}
}


void MipImageTask::FrameEnd()
{

}


void MipImageTask::Shutdown()
{
	for ( uint32_t i = 0; i < m_image->info.mipLevels; i++ )
	{
		m_frameBuffers[ i ].Destroy();
		m_views[ i ].Destroy();
		if ( m_passes[ i ] != nullptr ) {
			delete m_passes[ i ];
		}
	}
	m_buffer.Destroy();

	m_tempImage.gpuImage->Destroy();
	delete m_tempImage.gpuImage;
}


void MipImageTask::Execute( CommandContext& context )
{
	if( m_mode == DOWNSAMPLE_LINEAR )
	{
		Transition( &context, *m_image, GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_DST );
		GenerateMipmaps( &context, *m_image );
	}
	else
	{
		if ( m_firstFrame ) {
			Transition( &context, m_tempImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
			m_firstFrame = false;
		}
		GenerateDownsampleMips( &context, m_views, m_passes, m_mode );
	}
}


uint32_t RenderSchedule::PendingTasks() const
{
	return ( static_cast<uint32_t>( tasks.size() ) - currentTask );
}


void RenderSchedule::Clear()
{
	for( size_t i = 0; i < tasks.size(); ++i ) {
		delete tasks[ i ];
	}
	tasks.clear();
}


void RenderSchedule::Queue( GpuTask* task )
{
	// FIXME: must own pointer
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