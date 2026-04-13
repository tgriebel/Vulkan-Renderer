#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include "../scene/entity.h"
#include "../globals/assetDefs.h"

#include "swapChain.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"
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
#ifdef USE_VULKAN
	ImGui_ImplVulkan_Shutdown();
#endif
#ifdef USE_GLFW
	ImGui_ImplGlfw_Shutdown();
#endif
	ImGui::DestroyContext();
#endif
}

void Renderer::ShutdownShaderResources()
{
	// Managed Cleanup
	RenderResource::Cleanup( resourceLifeTime_t::REBOOT );

	// Images
	const uint32_t textureCount = TextureLib().Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const Image& texture = TextureLib().Find( i )->Get();
		delete texture.gpuImage;
	}

	for ( uint32_t i = 0; i < MaxImageDescriptors; ++i )
	{
		resources.gpuImages2D.BindIndex( i, nullptr );
		resources.gpuImagesCube.BindIndex( i, nullptr );
	}

	// PSO
	DestroyPipelineCache();

	const uint32_t shaderCount = GpuProgramLib().Count();
	for ( uint32_t i = 0; i < shaderCount; ++i )
	{
		GpuProgram& prog = GpuProgramLib().Find( i )->Get();
		for ( uint32_t permIx = 0; permIx < prog.permCount; ++permIx )
		{
			for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx )
			{
				vkDestroyShaderModule( context.device, prog.vk_shaders[ permIx ][ shaderIx ], nullptr );
			}
		}
	}

	renderContext.FreeRegisteredBindParms();
}
