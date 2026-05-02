#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include "../scene/entity.h"
#include "../globals/assetDefs.h"
#include "../render_core/allocator.h"

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

	ShutdownShaderResources();

	uploader.Shutdown();

	// Sync
	gfxContext.presentSemaphore.Destroy();
	gfxContext.renderFinishedSemaphore.Destroy();
	computeContext.semaphore.Destroy();

	for ( size_t i = 0; i < MaxFrameStates; ++i ) {
		gfxContext.frameFence[ i ].Destroy();
	}

	schedule->Clear();

	AllocatorMemory::DestroyVmaAllocator();

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
	const uint32_t textureCount = ImageLib().Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const Image& texture = ImageLib().Find( i )->Get();
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

		prog.DestroyApiObjects();
	}

	renderContext.FreeRegisteredBindParms();
}
