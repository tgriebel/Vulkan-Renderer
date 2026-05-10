#include "imguiTask.h"
#include "../render_core/renderer.h"
#include "../render_state/cmdContext.h"
#include "../render_binding/bindings.h"
#include "../render_resources/gpuBuffer.h"
#include "../draw_passes/postPass.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/assetLib.h"
#include "../globals/assetDefs.h"

#include "imageShaderTask.h"

namespace
{
	struct imageStatParms_t
	{
		vec4f		dimensions;		// .xy = w/h, .z = lod
		vec2f		luminanceRange;	// log2 min/max
		uint32_t	imageId;
		uint32_t	baseOffset;
	};
}

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

#include "../app/imguiInterface.h"

extern imguiControls_t g_imguiControls;
#endif

struct imguiTaskRenderData_t
{
	CommandList*		commandContext;
	RenderContext*		renderContext;
	const DrawPass*		pass;
};

const uint32_t MaxImguiCallbackImages = 32;

static uint32_t pendingCallbackTasks = 0;
static imguiImageCallbackData_t callbackTasks[ MaxImguiCallbackImages ];
static imguiTaskRenderData_t renderTaskData; // Push/pop state


void ImguiImage2DRenderCallback( const ImDrawList* parentList, const ImDrawCmd* cmd )
{
	imguiImageCallbackData_t* callbackData = (imguiImageCallbackData_t*)cmd->UserCallbackData;

	shaderPermId_t permSet = static_cast<shaderPermId_t>( callbackData->permSet );

	hdl_t pipeLineHdl = FindPipelineObject( renderTaskData.pass, *callbackData->progAsset, permSet );

	if( pipeLineHdl == INVALID_HDL ) {
		pipeLineHdl = CreateGraphicsPipeline( renderTaskData.pass, *callbackData->progAsset, permSet );
	}

	const float visMinX = Max( callbackData->x, cmd->ClipRect.x );
	const float visMinY = Max( callbackData->y, cmd->ClipRect.y );
	const float visMaxX = Min( callbackData->x + callbackData->width,  cmd->ClipRect.z );
	const float visMaxY = Min( callbackData->y + callbackData->height, cmd->ClipRect.w );

	if ( visMinX >= visMaxX || visMinY >= visMaxY ) {
		return;
	}

	vk_QuadDraw( *renderTaskData.commandContext, pipeLineHdl, vec2f( visMinX, visMinY ), vec2f( visMaxX - visMinX, visMaxY - visMinY ), renderTaskData.pass );
}


void AddImguiCallback( ImDrawList* dl, const imguiImageCallbackData_t& callbackData )
{
	if ( pendingCallbackTasks >= MaxImguiCallbackImages ) {
		return;
	}

	callbackTasks[ pendingCallbackTasks ] = callbackData;

	dl->AddCallback( ImguiImage2DRenderCallback, &callbackTasks[ pendingCallbackTasks ] );
	dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );

	++pendingCallbackTasks;
}


void ImguiTask::Init( const DrawPass* pass, RenderContext* renderContext, ResourceContext* resourceContext, const bool finalizeImage )
{
	m_context = renderContext;
	m_resources = resourceContext;

	m_imguiPass = pass;

	m_transitionState.flags.presentAfter = finalizeImage;
	m_transitionState.flags.store = true;

	m_imagePass = new PostPass( m_context, const_cast<FrameBuffer*>( m_imguiPass->GetFrameBuffer() ) );

	m_imagePass->parms = m_context->RegisterBindParm( bindset_imageProcess );

	m_imagePass->codeImages.SetRenderContext( m_context );
	m_imagePass->codeCubeImages.SetRenderContext( m_context );

	m_imagePass->codeImages.Resize( 1 );
	m_imagePass->codeCubeImages.Resize( 1 );

	for ( uint32_t codeImageIx = 0; codeImageIx < 1; ++codeImageIx )
	{
		m_imagePass->codeImages.BindIndex( codeImageIx, rc.defaultImage );
		m_imagePass->codeCubeImages.BindIndex( codeImageIx, rc.defaultImageCube );
	}
	m_buffer.Create( "ImguiCallbackBuffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::UNMANAGED, 1, MaxBufferSizeInBytes, bufferType_t::UNIFORM );

	// MULTI_FRAME so each frame writes its own slot; we read the slot whose
	// dispatch finished MaxFrameStates frames ago — no FlushGPU stall needed.
	m_imageStatBuffer.Create(
		"ImageStatHistogram",
		swapBuffering_t::MULTI_FRAME,
		resourceLifeTime_t::UNMANAGED,
		ImageStatHistogramBins,
		sizeof( uint32_t ),
		bufferType_t::STORAGE );

	m_imageStatParmsBuffer.Create(
		"ImageStatParms",
		swapBuffering_t::SINGLE_FRAME,
		resourceLifeTime_t::UNMANAGED,
		1,
		sizeof( imageStatParms_t ),
		bufferType_t::UNIFORM );

	m_imageStatImages.SetRenderContext( m_context );
	m_imageStatImages.Resize( 1 );
	m_imageStatImages.BindIndex( 0, rc.defaultImage );

	m_imageStatParms      = m_context->RegisterBindParm( m_context->LookupBindSet( bindset_compute ) );
	m_imageStatImage      = nullptr;
	m_imageStatDispatched = false;
	memset( m_imageStatHistogram, 0, sizeof( m_imageStatHistogram ) );

	const Asset<GpuProgram>* progAsset = GpuProgramLib().Find( "ImageState" );
	CreateComputePipeline( *progAsset );
}


void ImguiTask::Shutdown()
{
	if( m_imagePass != nullptr )
	{
		delete m_imagePass;
		m_imagePass = nullptr;
	}
	m_buffer.Destroy();
	m_imageStatBuffer.Destroy();
	m_imageStatParmsBuffer.Destroy();
}


void ImguiTask::FrameBegin()
{
	struct viewerShaderConstants_t : public ImageShaderTask::baseConstants_t
	{
		vec4f		scissorRectUv;
		vec4f		tint;
		float		rangeMin;
		float		rangeMax;
		uint32_t	flags;
		uint32_t	sampleIndex;
	};

	const viewport_t viewport = m_imguiPass->GetViewport();

	m_imagePass->codeImages.BindIndex( 0, rc.redImage );
	m_imagePass->codeCubeImages.BindIndex( 0, rc.defaultImageCube );

	const Image* image = callbackTasks[ 0 ].image;

	if ( image != nullptr )
	{
		const bool isCubeImage = ( image->info.type == IMAGE_TYPE_CUBE );

		viewerShaderConstants_t constants{};

		constants.dimensions = vec4f( (float)image->info.width, (float)image->info.height, 1.0f / image->info.width, 1.0f / image->info.height );
		constants.level = callbackTasks[ 0 ].mipLevel;
		constants.mipCount = image->info.mipLevels;
		constants.layerCount = image->info.layers;
		constants.layer = callbackTasks[ 0 ].layer;
		constants.scissorRectUv = vec4f( callbackTasks[ 0 ].x, callbackTasks[ 0 ].y, callbackTasks[ 0 ].width, callbackTasks[ 0 ].height );
		
		constants.scissorRectUv.x *= 1.0f / viewport.width;
		constants.scissorRectUv.y *= 1.0f / viewport.height;
		constants.scissorRectUv.z *= 1.0f / viewport.width;
		constants.scissorRectUv.w *= 1.0f / viewport.height;

		constants.tint = callbackTasks[ 0 ].tint;
		constants.rangeMin = callbackTasks[ 0 ].rangeMin;
		constants.rangeMax = callbackTasks[ 0 ].rangeMax;
		constants.flags = ( isCubeImage ? 0x01 : 0x00 ) | callbackTasks[ 0 ].flags;
		constants.sampleIndex = callbackTasks[ 0 ].sampleIndex;

		m_buffer.SetPos( 0 );
		m_buffer.CopyData( &constants, sizeof( constants ) );

		if( isCubeImage ) {
			m_imagePass->codeCubeImages.BindIndex( 0, image );
		} else {
			m_imagePass->codeImages.BindIndex( 0, image );
		}		
	}

	m_imagePass->parms->Bind( BINDING_NAME( sourceImages ),		&m_imagePass->codeImages );
	m_imagePass->parms->Bind( BINDING_NAME( sourceCubeImages ),	m_imagePass->codeCubeImages[ 0 ] );
	m_imagePass->parms->Bind( BINDING_NAME( imageStencil ),		rc.whiteImage );
	m_imagePass->parms->Bind( BINDING_NAME( imageProcess ),		&m_buffer );

	// Image histogram setup — only run when a non-cube image is being viewed.
	m_imageStatImage      = nullptr;
	m_imageStatDispatched = false;

	if ( image != nullptr && image->info.type != IMAGE_TYPE_CUBE )
	{
		imageStatParms_t parms{};
		parms.dimensions     = vec4f( (float)image->info.width, (float)image->info.height, (float)callbackTasks[ 0 ].mipLevel, 0.0f );
		parms.luminanceRange = vec2f( -8.0f, 4.0f );
		parms.imageId        = 0;
		parms.baseOffset     = 0;

		m_imageStatParmsBuffer.SetPos( 0 );
		m_imageStatParmsBuffer.CopyData( &parms, sizeof( parms ) );

		// The current frame's slot holds the result from MaxFrameStates frames
		// ago — guaranteed complete since the GPU is at most MaxFrameStates - 1
		// frames in flight. Read it now, then overwrite for this frame's dispatch.
		assert( m_imageStatBuffer.VisibleToCpu() );
		memcpy( m_imageStatHistogram, m_imageStatBuffer.Get(), sizeof( m_imageStatHistogram ) );
		memset( m_imageStatBuffer.Get(), 0, ImageStatHistogramBins * sizeof( uint32_t ) );

		m_imageStatImages.BindIndex( 0, image );

		m_imageStatImage      = image;
		m_imageStatDispatched = true;
	}

	m_imageStatParms->Bind( BINDING_NAME( globalsBuffer ), &m_resources->globalConstants );
	m_imageStatParms->Bind( BINDING_NAME( computeImage ),  &m_imageStatImages );
	m_imageStatParms->Bind( BINDING_NAME( computeParms ),  &m_imageStatParmsBuffer );
	m_imageStatParms->Bind( BINDING_NAME( computeWrite ),  &m_imageStatBuffer );

#ifdef USE_IMGUI
	// Prepare dear imgui render data
	ImGui::Render();
#endif
}


void ImguiTask::FrameEnd()
{
	m_imageStatDispatched = false;
}


void ImguiTask::Resize()
{

}


std::string ImguiTask::AsString() const
{
	std::stringstream ss;
	ss << "ImguiTask";
	return ss.str();
}


void ImguiTask::Execute( CommandList& cmdContext )
{
	cmdContext.MarkerBeginRegion( "ImGui", ColorToVector( Color::White ) );

	// Image histogram dispatch — must execute outside any rendering scope.
	if ( m_imageStatDispatched && m_imageStatImage != nullptr )
	{
		const uint32_t groupSize = 16;
		const uint32_t groupsX = CommandList::DispatchDim( m_imageStatImage->info.width, groupSize );
		const uint32_t groupsY = CommandList::DispatchDim( m_imageStatImage->info.width, groupSize );

		const hdl_t progHdl = AssetLib<GpuProgram>::Handle( "ImageState" );
		cmdContext.Dispatch( progHdl, *m_imageStatParms, groupsX, groupsY, 1 );
	}

#ifdef USE_IMGUI
	const FrameBuffer* fb = m_imguiPass->GetFrameBuffer();
	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView   = fb->GetColor()->gpuImage->GetVkImageView( context.bufferId );
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset    = { m_imguiPass->GetViewport().x, m_imguiPass->GetViewport().y };
	renderingInfo.renderArea.extent    = { m_imguiPass->GetViewport().width, m_imguiPass->GetViewport().height };
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &colorAttachment;

	vkCmdBeginRendering( cmdBuffer, &renderingInfo );

	renderTaskData.commandContext = &cmdContext;
	renderTaskData.renderContext = m_context;
	renderTaskData.pass = m_imagePass;

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );

	renderTaskData.commandContext = nullptr;
	renderTaskData.renderContext = nullptr;
	renderTaskData.pass = nullptr;

	pendingCallbackTasks = 0;

	vkCmdEndRendering( cmdBuffer );
#endif

	cmdContext.MarkerEndRegion();
}
