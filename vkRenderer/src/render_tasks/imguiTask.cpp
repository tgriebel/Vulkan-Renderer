/*
* MIT License
*
* Copyright( c ) 2025 Thomas Griebel
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

#include "imguiTask.h"
#include "../render_core/renderer.h"
#include "../render_state/cmdContext.h"
#include "../render_binding/bindings.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"

extern imguiControls_t			g_imguiControls;
#endif

struct imguiTaskRenderData_t
{
	CommandContext*		commandContext;
	const DrawPass*		pass;
};

static imguiTaskRenderData_t renderTaskData;


void ImguiImage2DRenderCallback( const ImDrawList* parentList, const ImDrawCmd* cmd )
{
	imguiImageCallbackData_t* callbackData = (imguiImageCallbackData_t*)cmd->UserCallbackData;

	pipelineObject_t* pipelineObject = nullptr;

	const hdl_t pipeLine = FindPipelineObject( renderTaskData.pass, *callbackData->progAsset );

	vec2f topCorner = vec2f( callbackData->x, callbackData->y );
	vec2f bottomCorner = vec2f( callbackData->width, callbackData->height );

	vk_QuadDraw( *renderTaskData.commandContext, pipeLine, topCorner, bottomCorner, renderTaskData.pass );
}


void ImguiTask::Init( const DrawPass* pass, RenderContext* renderContext, ResourceContext* resourceContext, const bool finalizeImage )
{
	m_context = renderContext;
	m_resources = resourceContext;

	m_imguiPass = pass;

	m_transitionState.flags.presentAfter = finalizeImage;
	m_transitionState.flags.store = true;

	m_imagePass = new PostPass( const_cast<FrameBuffer*>( m_imguiPass->GetFrameBuffer() ) );

	m_imagePass->parms = m_context->RegisterBindParm( bindset_imageProcess );

	m_imagePass->codeImages.Resize( 1 );
	m_imagePass->codeCubeImages.Resize( 1 );

	for ( uint32_t codeImageIx = 0; codeImageIx < 1; ++codeImageIx ) {
		m_imagePass->codeImages[ codeImageIx ] = rc.redImage;
		m_imagePass->codeCubeImages[ codeImageIx ] = rc.defaultImageCube;
	}

	m_buffer.Create( "Resource buffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::UNMANAGED, 1, MaxBufferSizeInBytes, bufferType_t::UNIFORM, m_context->sharedMemory );
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
		uint32_t textureId;
	};

	viewerShaderConstants_t constants{};
	constants.dimensions = vec4f( 100.0f, 100.0f, 1.0f / 100.0f, 1.0f / 100.0f );
	constants.mipCount = 1;
	constants.layerCount = 1;
	constants.textureId = g_imguiControls.dbgImageId; // FIXME: better data flow

	m_buffer.SetPos( 0 );
	m_buffer.CopyData( &constants, sizeof( constants ) );

	m_imagePass->parms->Bind( bind_sourceImages, m_imagePass->codeImages.Count() > 0 ? &m_imagePass->codeImages : &rc.defaultImageArray );
	m_imagePass->parms->Bind( bind_sourceCubeImages, m_imagePass->codeCubeImages.Count() > 0 ? m_imagePass->codeCubeImages[ 0 ] : rc.defaultImageCube );
	m_imagePass->parms->Bind( bind_imageStencil, &m_resources->stencilImageView );
	m_imagePass->parms->Bind( bind_imageProcess, &m_buffer );

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

	vkCmdEndRenderPass( cmdBuffer );
#endif

	cmdContext.MarkerEndRegion();
}