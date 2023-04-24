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
#include <scene/entity.h>

extern AssetLib< pipelineObject_t > pipelineLib; // TODO: move to renderer

void Renderer::Cleanup()
{
	DestroyFrameResources();
	swapChain.Destroy();

	ShutdownImGui();

	// Memory
	vkDestroyDescriptorPool( context.device, descriptorPool, nullptr );

	// Buffers
	vkFreeCommandBuffers( context.device, graphicsQueue.commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), graphicsQueue.commandBuffers );
	vkFreeCommandBuffers( context.device, computeQueue.commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), computeQueue.commandBuffers );

	ShutdownShaderResources();

	vkDestroySampler( context.device, vk_bilinearSampler, nullptr );
	vkDestroySampler( context.device, vk_depthShadowSampler, nullptr );

	// Shader binding resources
	defaultBindSet.Destroy();
	particleShaderBinds.Destroy();

	// Sync
	for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
	{
		vkDestroySemaphore( context.device, graphicsQueue.renderFinishedSemaphores[ i ], nullptr );
		vkDestroySemaphore( context.device, graphicsQueue.imageAvailableSemaphores[ i ], nullptr );
		vkDestroyFence( context.device, graphicsQueue.inFlightFences[ i ], nullptr );

		vkDestroySemaphore( context.device, computeQueue.semaphores[ i ], nullptr );
	}

	vkDestroyCommandPool( context.device, graphicsQueue.commandPool, nullptr );
	vkDestroyCommandPool( context.device, computeQueue.commandPool, nullptr );
	vkDestroyDevice( context.device, nullptr );

	if ( enableValidationLayers )
	{
		DestroyDebugUtilsMessengerEXT( context.instance, debugMessenger, nullptr );
	}

	vkDestroySurfaceKHR( context.instance, gWindow.vk_surface, nullptr );
	vkDestroyInstance( context.instance, nullptr );

	gWindow.~Window();
}


void Renderer::DestroyFrameResources()
{
	vkDestroyRenderPass( context.device, mainPassState.pass, nullptr );
	vkDestroyRenderPass( context.device, shadowPassState.pass, nullptr );
	vkDestroyRenderPass( context.device, postPassState.pass, nullptr );

	for ( size_t frameId = 0; frameId < MAX_FRAMES_STATES; ++frameId )
	{
		// Images
		frameState[ frameId ].viewColorImage.Destroy();
		frameState[ frameId ].shadowMapImage.Destroy();
		frameState[ frameId ].depthImage.Destroy();
		frameState[ frameId ].stencilImage.Destroy();

		// Buffers
		vkDestroyFramebuffer( context.device, shadowMap.buffer[ frameId ], nullptr );
		vkDestroyFramebuffer( context.device, mainColor.buffer[ frameId ], nullptr );
		vkDestroyFramebuffer( context.device, viewColor.buffer[ frameId ], nullptr );
	}

	vkFreeMemory( context.device, frameBufferMemory.GetMemoryResource(), nullptr );
	frameBufferMemory.Unbind();
	//vkUnmapMemory
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
	rc.whiteImage.Destroy();
	rc.blackImage.Destroy();

	const uint32_t textureCount = gAssets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const Texture& texture = gAssets.textureLib.Find( i )->Get();
		texture.gpuImage->Destroy();
		delete texture.gpuImage;
	}

	// PSO
	const uint32_t psoCount = pipelineLib.Count();
	for ( uint32_t i = 0; i < psoCount; ++i )
	{
		Asset<pipelineObject_t>* psoAsset = pipelineLib.Find( i );
		vkDestroyPipeline( context.device, psoAsset->Get().pipeline, nullptr );
		vkDestroyPipelineLayout( context.device, psoAsset->Get().pipelineLayout, nullptr );
	}

	const uint32_t shaderCount = gAssets.gpuPrograms.Count();
	for ( uint32_t i = 0; i < shaderCount; ++i )
	{
		Asset<GpuProgram>* shaderAsset = gAssets.gpuPrograms.Find( i );
		vkDestroyShaderModule( context.device, shaderAsset->Get().vk_shaders[ 0 ], nullptr );
		vkDestroyShaderModule( context.device, shaderAsset->Get().vk_shaders[ 1 ], nullptr );
	}
}