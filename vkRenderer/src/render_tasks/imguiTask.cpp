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

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"

extern imguiControls_t			g_imguiControls;
#endif


void ImguiTask::Init( const DrawPass* pass, const bool finalizeImage )
{
	m_pass = pass;

	m_transitionState.flags.presentAfter = finalizeImage;
	m_transitionState.flags.store = true;
}


void ImguiTask::Shutdown()
{
}


void ImguiTask::FrameBegin()
{
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
	passInfo.renderPass = m_pass->GetFrameBuffer()->GetVkRenderPass( m_transitionState );
	passInfo.framebuffer = m_pass->GetFrameBuffer()->GetVkBuffer( m_transitionState, context.bufferId );
	passInfo.renderArea.offset = { m_pass->GetViewport().x, m_pass->GetViewport().y };
	passInfo.renderArea.extent = { m_pass->GetViewport().width, m_pass->GetViewport().height };

	passInfo.clearValueCount = 0;
	passInfo.pClearValues = nullptr;

	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	vkCmdBeginRenderPass( cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE );

	ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmdBuffer );

	vkCmdEndRenderPass( cmdBuffer );
#endif

	cmdContext.MarkerEndRegion();
}