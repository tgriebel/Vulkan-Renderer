#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include <scene/entity.h>

void Renderer::Cleanup()
{
	DestroyFrameResources();
	swapChain.Destroy();

	ShutdownImGui();

	vkFreeMemory( context.device, localMemory.GetMemoryResource(), nullptr );
	vkFreeMemory( context.device, sharedMemory.GetMemoryResource(), nullptr );
	vkFreeMemory( context.device, frameBufferMemory.GetMemoryResource(), nullptr );
	localMemory.Unbind();
	sharedMemory.Unbind();
	frameBufferMemory.Unbind();
	vkDestroyDescriptorPool( context.device, descriptorPool, nullptr );

	vkFreeCommandBuffers( context.device, graphicsQueue.commandPool, static_cast<uint32_t>( MAX_FRAMES_STATES ), graphicsQueue.commandBuffers );
	vkFreeCommandBuffers( context.device, computeQueue.commandPool, 1, &computeQueue.commandBuffer );

	for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
		vkDestroyBuffer( context.device, frameState[ i ].globalConstants.GetVkObject(), nullptr );
		vkDestroyBuffer( context.device, frameState[ i ].surfParms.GetVkObject(), nullptr );
		vkDestroyBuffer( context.device, frameState[ i ].materialBuffers.GetVkObject(), nullptr );
		vkDestroyBuffer( context.device, frameState[ i ].lightParms.GetVkObject(), nullptr );
	}

	const uint32_t textureCount = gAssets.textureLib.Count();
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const Texture& texture = gAssets.textureLib.Find( i )->Get();
		vkDestroyImageView( context.device, texture.gpuImage.vk_view, nullptr );
		vkDestroyImage( context.device, texture.gpuImage.vk_image, nullptr );
	}

	vkDestroySampler( context.device, vk_bilinearSampler, nullptr );
	vkDestroySampler( context.device, vk_depthShadowSampler, nullptr );

	for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
	{
		vkDestroySemaphore( context.device, graphicsQueue.renderFinishedSemaphores[ i ], nullptr );
		vkDestroySemaphore( context.device, graphicsQueue.imageAvailableSemaphores[ i ], nullptr );
		vkDestroyFence( context.device, graphicsQueue.inFlightFences[ i ], nullptr );
	}
	vkDestroySemaphore( context.device, computeQueue.semaphore, nullptr );

	vkDestroyCommandPool( context.device, graphicsQueue.commandPool, nullptr );
	vkDestroyCommandPool( context.device, computeQueue.commandPool, nullptr );
	vkDestroyDevice( context.device, nullptr );

	if ( enableValidationLayers )
	{
		DestroyDebugUtilsMessengerEXT( context.instance, debugMessenger, nullptr );
	}

	vkDestroySurfaceKHR( context.instance, window.vk_surface, nullptr );
	vkDestroyInstance( context.instance, nullptr );

	window.~Window();
}


void Renderer::DestroyFrameResources()
{
	vkDestroyRenderPass( context.device, mainPassState.pass, nullptr );
	vkDestroyRenderPass( context.device, shadowPassState.pass, nullptr );
	vkDestroyRenderPass( context.device, postPassState.pass, nullptr );

	for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
	{
		// Views
		vkDestroyImageView( context.device, frameState[ i ].viewColorImage.vk_view, nullptr );
		vkDestroyImageView( context.device, frameState[ i ].shadowMapImage.vk_view, nullptr );
		vkDestroyImageView( context.device, frameState[ i ].depthImage.vk_view, nullptr );

		// Images
		vkDestroyImage( context.device, frameState[ i ].viewColorImage.vk_image, nullptr );
		vkDestroyImage( context.device, frameState[ i ].shadowMapImage.vk_image, nullptr );
		vkDestroyImage( context.device, frameState[ i ].depthImage.vk_image, nullptr );

		// Passes
		vkDestroyFramebuffer( context.device, shadowPassState.fb[ i ], nullptr );
		vkDestroyFramebuffer( context.device, mainPassState.fb[ i ], nullptr );
		vkDestroyFramebuffer( context.device, postPassState.fb[ i ], nullptr );

		frameState[ i ].viewColorImage.allocation.Free();
		frameState[ i ].shadowMapImage.allocation.Free();
		frameState[ i ].depthImage.allocation.Free();
	}
}


void Renderer::ShutdownImGui()
{
#if defined( USE_IMGUI )
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
#endif
}