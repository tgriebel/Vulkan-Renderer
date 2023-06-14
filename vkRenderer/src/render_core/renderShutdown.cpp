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
	vkDeviceWaitIdle( context.device );
	Cleanup();
}


void Renderer::Cleanup()
{
	DestroyFramebuffers();
	g_swapChain.Destroy();

	ShutdownImGui();

	// Buffers
	vkFreeCommandBuffers( context.device, gfxContext.commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), gfxContext.commandBuffers );
	vkFreeCommandBuffers( context.device, computeContext.commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), computeContext.commandBuffers );
	vkFreeCommandBuffers( context.device, uploadContext.commandPool, 1, &uploadContext.commandBuffer );

	ShutdownShaderResources();

	vkDestroySampler( context.device, vk_bilinearSampler, nullptr );
	vkDestroySampler( context.device, vk_depthShadowSampler, nullptr );

	// Sync
	for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
	{
		vkDestroySemaphore( context.device, gfxContext.renderFinishedSemaphores[ i ], nullptr );
		vkDestroySemaphore( context.device, gfxContext.imageAvailableSemaphores[ i ], nullptr );
		vkDestroyFence( context.device, gfxContext.inFlightFences[ i ], nullptr );

		vkDestroySemaphore( context.device, computeContext.semaphores[ i ], nullptr );
	}

	// Memory
	vkDestroyDescriptorPool( context.device, descriptorPool, nullptr );

	vkDestroyCommandPool( context.device, gfxContext.commandPool, nullptr );
	vkDestroyCommandPool( context.device, computeContext.commandPool, nullptr );
	vkDestroyCommandPool( context.device, uploadContext.commandPool, nullptr );
	vkDestroyDevice( context.device, nullptr );

	if ( enableValidationLayers )
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( context.instance, "vkDestroyDebugUtilsMessengerEXT" );
		if ( func != nullptr ) {
			func( context.instance, debugMessenger, nullptr );
		}
	}

	vkDestroySurfaceKHR( context.instance, g_window.vk_surface, nullptr );
	vkDestroyInstance( context.instance, nullptr );

	g_window.~Window();
}


void Renderer::DestroyFramebuffers()
{
	for ( size_t frameId = 0; frameId < MAX_FRAMES_STATES; ++frameId )
	{
		frameState[ frameId ].depthImageView.Destroy();
		frameState[ frameId ].stencilImageView.Destroy();

		delete frameState[ frameId ].viewColorImage.gpuImage;
		for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx ) {
			delete frameState[ frameId ].shadowMapImage[ shadowIx ].gpuImage;
		}
		delete frameState[ frameId ].depthStencilImage.gpuImage;
	}

	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx ) {
		shadowMap[ shadowIx ].Destroy();
	}
	mainColor.Destroy();

	vkFreeMemory( context.device, frameBufferMemory.GetMemoryResource(), nullptr );
	frameBufferMemory.Unbind();
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
	vkFreeMemory( context.device, localMemory.GetMemoryResource(), nullptr );
	vkFreeMemory( context.device, sharedMemory.GetMemoryResource(), nullptr );
	localMemory.Unbind();
	sharedMemory.Unbind();
	localMemory.Reset();
	sharedMemory.Reset();

	// Staging
	stagingBuffer.Destroy();

	// Buffers
	for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
		frameState[ i ].globalConstants.Destroy();
		frameState[ i ].viewParms.Destroy();
		frameState[ i ].surfParms.Destroy();
		frameState[ i ].materialBuffers.Destroy();
		frameState[ i ].lightParms.Destroy();
		frameState[ i ].particleBuffer.Destroy();
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