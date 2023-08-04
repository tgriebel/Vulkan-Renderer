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
	Cleanup();
}


void Renderer::Cleanup()
{
	DestroyFramebuffers();
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

	context.Destroy( g_window );
	
	g_window.~Window();
}


void Renderer::DestroyFramebuffers()
{
	frameState.depthImageView.Destroy();
	frameState.stencilImageView.Destroy();

	delete tempColorImage.gpuImage;
	delete mainColorImage.gpuImage;
	delete depthStencilImage.gpuImage;

	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
	{
		delete shadowMapImage[ shadowIx ].gpuImage;
		shadowMap[ shadowIx ].Destroy();
	}
	mainColor.Destroy();

	frameBufferMemory.Destroy();
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
	ib.Destroy();
	vb.Destroy();

	// Memory
	localMemory.Destroy();
	sharedMemory.Destroy();

	// Staging
	geoStagingBuffer.Destroy();
	textureStagingBuffer.Destroy();

	// Buffers
	{
		frameState.globalConstants.Destroy();
		frameState.viewParms.Destroy();
		frameState.surfParms.Destroy();
		frameState.materialBuffers.Destroy();
		frameState.lightParms.Destroy();
		frameState.particleBuffer.Destroy();
	}

	// Images
	delete rc.whiteImage.gpuImage;
	delete rc.blackImage.gpuImage;

	const uint32_t textureCount = g_assets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const Image& texture = g_assets.textureLib.Find( i )->Get();
		delete texture.gpuImage;
	}

	for ( uint32_t i = 0; i < MaxImageDescriptors; ++i )
	{
		gpuImages2D[ i ] = nullptr;
		gpuImagesCube[ i ] = nullptr;
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
	defaultBindSet.Destroy();
	particleShaderBinds.Destroy();

	FreeRegisteredBindParms();
	bindParmsList.Reset();
}