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

#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include <gfxcore/scene/entity.h>

#include "swapChain.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif

void Renderer::Shutdown()
{
	FlushGPU();
	Destroy();
}


void Renderer::Destroy()
{
	vk_ClearRenderPassCache();

	RenderResource::Cleanup( resourceLifeTime_t::RESIZE );
	g_swapChain.Destroy();

	ShutdownImGui();

	// Buffers
	gfxContext.Destroy();
	computeContext.Destroy();
	uploadContext.Destroy();

	ShutdownShaderResources();

	// Sync
	gfxContext.presentSemaphore.Destroy();
	gfxContext.renderFinishedSemaphore.Destroy();
	computeContext.semaphore.Destroy();

	uploadFinishedSemaphore.Destroy();

	for ( size_t i = 0; i < MaxFrameStates; ++i ) {
		gfxContext.frameFence[ i ].Destroy();
	}

	schedule.Clear();

	context.Destroy( g_window );
	
	g_window.~Window();
}


void Renderer::ShutdownImGui()
{
#if defined( USE_IMGUI )
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
#endif
}

void Renderer::ShutdownShaderResources()
{
	// Managed Cleanup
	RenderResource::Cleanup( resourceLifeTime_t::REBOOT );

	// Images
	const uint32_t textureCount = g_assets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const Image& texture = g_assets.textureLib.Find( i )->Get();
		delete texture.gpuImage;
	}

	for ( uint32_t i = 0; i < MaxImageDescriptors; ++i )
	{
		resources.gpuImages2D[ i ] = nullptr;
		resources.gpuImagesCube[ i ] = nullptr;
	}

	// PSO
	DestroyPipelineCache();

	const uint32_t shaderCount = g_assets.gpuPrograms.Count();
	for ( uint32_t i = 0; i < shaderCount; ++i )
	{
		Asset<GpuProgram>* shaderAsset = g_assets.gpuPrograms.Find( i );
		vkDestroyShaderModule( context.device, shaderAsset->Get().vk_shaders[ 0 ], nullptr );
		vkDestroyShaderModule( context.device, shaderAsset->Get().vk_shaders[ 1 ], nullptr );
	}

	// Shader binding resources
	for( auto bindSet : renderContext.bindSets ) {
		bindSet.second.Destroy();
	}

	renderContext.FreeRegisteredBindParms();
}