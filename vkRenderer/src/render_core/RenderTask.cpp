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

#include <SysCore/serializer.h>
#include <GfxCore/io/serializeClasses.h>

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


void ImageWritebackTask::Init()
{
	struct writeBackParms_t
	{
		vec4f dimensions;
	};

	writeBackParms_t writeBackParms{};

	const uint32_t maxBpp = sizeof( vec4f );
	const uint32_t elementsCount = m_imageArray[0]->info.width * m_imageArray[ 0 ]->info.height * m_imageArray.Count();
	m_writebackBuffer.Create( "Writeback Buffer", LIFETIME_PERSISTENT, elementsCount, maxBpp, bufferType_t::STORAGE, m_context->sharedMemory );
	m_resourceBuffer.Create( "Resource buffer", LIFETIME_TEMP, 1, sizeof( writeBackParms ), bufferType_t::UNIFORM, m_context->sharedMemory );

	writeBackParms.dimensions = vec4f( (float)m_imageArray[ 0 ]->info.width, (float)m_imageArray[ 0 ]->info.height, (float)m_imageArray.Count(), 0.0f );

	m_resourceBuffer.SetPos( 0 );
	m_resourceBuffer.CopyData( &writeBackParms, sizeof( writeBackParms ) );

	m_parms = m_context->RegisterBindParm( m_context->LookupBindSet( bindset_compute ) );
}


void ImageWritebackTask::FrameBegin()
{
	m_parms->Bind( bind_globalsBuffer, &m_resources->globalConstants );
	m_parms->Bind( bind_computeImage, &m_imageArray );
	m_parms->Bind( bind_computeParms, &m_resourceBuffer );
	m_parms->Bind( bind_computeWrite, &m_writebackBuffer );
}


void ImageWritebackTask::FrameEnd()
{
	FlushGPU();

	assert( m_screenshot == false ); // TODO: not implemented
	assert( m_writeToDiskOnFrameEnd ); // TODO: only functionality currently

	assert( m_writebackBuffer.VisibleToCpu() );

	Image img;
	img.info = m_imageArray[ 0 ]->info;
	img.info.type = IMAGE_TYPE_CUBE;
	img.info.fmt = IMAGE_FMT_RGBA_16;
	img.info.layers = 6;
	img.cpuImage = new ImageBuffer<rgbaTupleh_t>();

	img.cpuImage->Init( m_imageArray[ 0 ]->info.width, m_imageArray[ 0 ]->info.height, m_imageArray.Count(), sizeof( rgbaTupleh_t ) );

	Serializer* s = new Serializer( img.cpuImage->GetByteCount() + 1024, serializeMode_t::STORE );

	float* floatData = reinterpret_cast<float*>( m_writebackBuffer.Get() );
	rgbaTupleh_t* convertedData = reinterpret_cast<rgbaTupleh_t*>( img.cpuImage->Ptr() );

	const uint32_t bufferLength = img.cpuImage->GetPixelCount();
	for( uint32_t i = 0; i < bufferLength; ++i )
	{
		rgbaTupleh_t rgba16;
		rgba16.r = PackFloat32( floatData[ 3 ] );
		rgba16.g = PackFloat32( floatData[ 2 ] );
		rgba16.b = PackFloat32( floatData[ 1 ] );
		rgba16.a = PackFloat32( floatData[ 0 ] );

		convertedData[ i ] = rgba16;
		floatData += 4;
	}

	img.Serialize( s );

	s->WriteFile( TexturePath + CodeAssetPath + m_fileName );

	delete s;
}



void ImageWritebackTask::Execute( CommandContext& context )
{
	// vkCmdCopyImageToBuffer
	// FIXME: barrier

	const uint32_t blockSize = 8;

	const hdl_t progHdl = AssetLibGpuProgram::Handle( "ImageWriteback" );
	context.Dispatch( progHdl, *m_parms, m_imageArray[ 0 ]->info.width / blockSize + 1, m_imageArray[ 0 ]->info.height / blockSize + 1, m_imageArray.Count() / blockSize + 1 );
}


void ImageWritebackTask::Shutdown()
{
	m_writebackBuffer.Destroy();
	m_resourceBuffer.Destroy();
}


void MipImageTask::Init( const mipProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "MipImageTaskInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	m_dbgName = info.name;
	m_image = info.img;
	m_mode = info.mode;
	m_context = info.context;
	m_resources = info.resources;

	m_context->scratchMemory.AdjustOffset( 0, 0 );

	const uint32_t mipLevels = m_image->info.mipLevels;

	m_passes.resize( mipLevels );
	m_views.resize( mipLevels );
	m_frameBuffers.resize( mipLevels );
	m_bufferViews.resize( mipLevels );

	{
		m_tempImage.info = m_image->info;
		m_tempImage.info.mipLevels = 1;
		m_tempImage.subResourceView.mipLevels = m_tempImage.info.mipLevels;
		m_tempImage.subResourceView.baseMip = 0;
		m_tempImage.subResourceView.arrayCount = m_tempImage.info.layers;
		m_tempImage.subResourceView.baseArray = 0;
		MipDimensions( 1, m_image->info.width, m_image->info.height, &m_tempImage.info.width, &m_tempImage.info.height );

		m_tempImage.gpuImage = new GpuImage();
		m_tempImage.gpuImage->Create( "tempMipImage", m_tempImage.info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, m_context->scratchMemory );
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
		info.color0 = &m_tempImage;
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