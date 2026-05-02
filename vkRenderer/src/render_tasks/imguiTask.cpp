#include "imguiTask.h"
#include "../render_core/renderer.h"
#include "../render_state/cmdContext.h"
#include "../render_binding/bindings.h"
#include "../draw_passes/postPass.h"

#include "imageShaderTask.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

#include "../app/imguiInterface.h"

extern imguiControls_t g_imguiControls;
#endif

struct imguiTaskRenderData_t
{
	CommandContext*		commandContext;
	const DrawPass*		pass;
};

const uint32_t MaxImguiCallbackImages = 32;

static uint32_t pendingCallbackTasks = 0;
static imguiImageCallbackData_t callbackTasks[ MaxImguiCallbackImages ];
static imguiTaskRenderData_t renderTaskData; // Push/pop state


void ImguiImage2DRenderCallback( const ImDrawList* parentList, const ImDrawCmd* cmd )
{
	imguiImageCallbackData_t* callbackData = (imguiImageCallbackData_t*)cmd->UserCallbackData;

	const hdl_t pipeLine = FindPipelineObject( renderTaskData.pass, *callbackData->progAsset, static_cast<shaderPermId_t>( callbackData->permSet ) );

	const float visMinX = Max( callbackData->x, cmd->ClipRect.x );
	const float visMinY = Max( callbackData->y, cmd->ClipRect.y );
	const float visMaxX = Min( callbackData->x + callbackData->width,  cmd->ClipRect.z );
	const float visMaxY = Min( callbackData->y + callbackData->height, cmd->ClipRect.w );

	if ( visMinX >= visMaxX || visMinY >= visMaxY ) {
		return;
	}

	vk_QuadDraw( *renderTaskData.commandContext, pipeLine, vec2f( visMinX, visMinY ), vec2f( visMaxX - visMinX, visMaxY - visMinY ), renderTaskData.pass );
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
	m_buffer.Create( "ImguiCallbackBuffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::UNMANAGED, 1, MaxBufferSizeInBytes, bufferType_t::UNIFORM, m_context->sharedMemory );
}


void ImguiTask::Shutdown()
{
	if( m_imagePass != nullptr )
	{
		delete m_imagePass;
		m_imagePass = nullptr;
	}
	m_buffer.Destroy();
}


void ImguiTask::FrameBegin()
{
	struct viewerShaderConstants_t : public ImageShaderTask::constants_t
	{
		vec4f		scissorRectUv;
		vec4f		tint;
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

		const float* t = callbackTasks[ 0 ].tint;
		constants.tint  = vec4f( t[ 0 ], t[ 1 ], t[ 2 ], t[ 3 ] );
		constants.flags       = ( isCubeImage ? 0x01 : 0x00 ) | callbackTasks[ 0 ].flags;
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
	m_imagePass->parms->Bind( BINDING_NAME( imageStencil ),		&m_resources->stencilImageView );
	m_imagePass->parms->Bind( BINDING_NAME( imageProcess ),		&m_buffer );

#ifdef USE_IMGUI
	// Prepare dear imgui render data
	ImGui::Render();
#endif
}


void ImguiTask::FrameEnd()
{

}


void ImguiTask::Resize()
{

}


std::string ImguiTask::AsString() const
{
	std::stringstream ss;
	ss << "<ImguiTask>";
	return ss.str();
}


void ImguiTask::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( "ImGui", ColorToVector( Color::White ) );

#ifdef USE_IMGUI
	VkRenderPassBeginInfo passInfo{ };
	passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passInfo.renderPass = m_imguiPass->GetFrameBuffer()->GetVkRenderPass( m_transitionState );
	passInfo.framebuffer = m_imguiPass->GetFrameBuffer()->GetVkBuffer( m_transitionState, context.bufferId );
	passInfo.renderArea.offset = { m_imguiPass->GetViewport().x, m_imguiPass->GetViewport().y };
	passInfo.renderArea.extent = { m_imguiPass->GetViewport().width, m_imguiPass->GetViewport().height };

	passInfo.clearValueCount = 0;
	passInfo.pClearValues = nullptr;

	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	renderTaskData.commandContext = &cmdContext;
	renderTaskData.pass = m_imagePass;

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );

	renderTaskData.commandContext = nullptr;
	renderTaskData.pass = nullptr;

	pendingCallbackTasks = 0;

	vkCmdEndRenderPass( cmdBuffer );
#endif

	cmdContext.MarkerEndRegion();
}
