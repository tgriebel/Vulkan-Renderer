#pragma once

#include <scene/camera.h>

#include "common.h"
#include "window.h"
#include "input.h"
#include "common.h"
#include "render_util.h"
#include "deviceContext.h"
#include "pipeline.h"
#include "swapChain.h"
#include "core/assetLib.h"
#include "FrameState.h"
#include "renderConstants.h"
#include "renderview.h"
#include "io.h"
#include "GeoBuilder.h"
#include <primitives/geom.h>
#include <resource_types/texture.h>
#include <resource_types/gpuProgram.h>
#include <scene/scene.h>

#include <raytracer/scene.h>
#include <raytracer/raytrace.h>
#include <SysCore/timer.h>

#if defined( USE_IMGUI )
#include "external/imgui/imgui.h"
#include "external/imgui/backends/imgui_impl_glfw.h"
#include "external/imgui/backends/imgui_impl_vulkan.h"
#endif

#include <scene/entity.h>

#if defined( USE_IMGUI )
static ImGui_ImplVulkanH_Window imguiMainWindowData;
extern imguiControls_t gImguiControls;
#endif

typedef AssetLib<pipelineObject_t>	AssetLibPipelines;
typedef AssetLib<GpuImage>			AssetLibGpuImages;

extern AssetLibPipelines			pipelineLib;
extern Scene						scene;
extern Window						gWindow;

class Renderer
{
public:
	void Init()
	{
		InitVulkan();
		InitImGui();
	}

	void RenderScene( Scene* scene );

	void Destroy()
	{
		vkDeviceWaitIdle( context.device );
		Cleanup();
	}

	bool IsReady() {
		return ( frameNumber > 0 );
	}

	static void	GenerateGpuPrograms( AssetLibGpuProgram& lib );

	void		CreatePipelineObjects();

	void		UploadAssets( AssetManager& assets );

private:

	static const uint32_t				ShadowMapWidth = 1024;
	static const uint32_t				ShadowMapHeight = 1024;

	static const bool					ValidateVerbose = false;
	static const bool					ValidateWarnings = false;
	static const bool					ValidateErrors = true;

	renderConstants_t					rc;
	RenderView							renderView;
	RenderView							shadowView;

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	Timer							frameTimer;
	float							renderTime = 0.0f;

	std::set<hdl_t>					uploadTextures;
	std::set<hdl_t>					uploadMaterials;

	VkDebugUtilsMessengerEXT		debugMessenger;
	SwapChain						swapChain;
	graphicsQueue_t					graphicsQueue;
	computeQueue_t					computeQueue;
	DrawPassState					shadowPassState;
	DrawPassState					mainPassState;
	DrawPassState					postPassState;
	VkDescriptorSetLayout			globalLayout;
	VkDescriptorSetLayout			postProcessLayout;
	VkDescriptorPool				descriptorPool;
	VkSampleCountFlagBits			msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	FrameState						frameState[ MAX_FRAMES_STATES ];
	size_t							frameId = 0;
	uint32_t						bufferId = 0;
	uint32_t						frameNumber = 0;
	GpuBuffer						stagingBuffer;
	GpuBuffer						vb;	// move
	GpuBuffer						ib;
	GpuImage						gpuImages[ MaxImageDescriptors ];
	materialBufferObject_t			materialBuffer[ MaxMaterialDescriptors ];

	uint32_t						imageFreeSlot = 0;
	uint32_t						materialFreeSlot = 0;
	uint32_t						vbBufElements = 0;
	uint32_t						ibBufElements = 0;

	AllocatorVkMemory				localMemory;
	AllocatorVkMemory				frameBufferMemory;
	AllocatorVkMemory				sharedMemory;

	VkSampler						vk_bilinearSampler;
	VkSampler						vk_depthShadowSampler;

	VkDescriptorBufferInfo			vk_globalConstantsInfo;
	VkDescriptorBufferInfo			vk_surfaceUbo[ MaxSurfaces ];
	VkDescriptorImageInfo			vk_image2DInfo[ MaxImageDescriptors ];
	VkDescriptorImageInfo			vk_shadowImageInfo[ MaxImageDescriptors ];
	VkDescriptorImageInfo			vk_codeImageInfo[ MaxCodeImages ];
	VkDescriptorImageInfo			vk_shadowCodeImageInfo[ MaxCodeImages ];
	VkDescriptorImageInfo			vk_imageCubeInfo[ MaxImageDescriptors ];
	VkDescriptorImageInfo			vk_postImageInfo[ MaxPostImageDescriptors ];
	VkDescriptorBufferInfo			vk_materialBufferInfo[ MaxMaterialDescriptors ];
	VkDescriptorBufferInfo			vk_lightBufferInfo[ MaxLights ];

	float							nearPlane = 1000.0f;
	float							farPlane = 0.1f;

	VkSampleCountFlagBits GetMaxUsableSampleCount()
	{
		/*
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties( context.physicalDevice, &physicalDeviceProperties );

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if ( counts & VK_SAMPLE_COUNT_64_BIT ) { return VK_SAMPLE_COUNT_64_BIT; }
		if ( counts & VK_SAMPLE_COUNT_32_BIT ) { return VK_SAMPLE_COUNT_32_BIT; }
		if ( counts & VK_SAMPLE_COUNT_16_BIT ) { return VK_SAMPLE_COUNT_16_BIT; }
		if ( counts & VK_SAMPLE_COUNT_8_BIT ) { return VK_SAMPLE_COUNT_8_BIT; }
		if ( counts & VK_SAMPLE_COUNT_4_BIT ) { return VK_SAMPLE_COUNT_4_BIT; }
		if ( counts & VK_SAMPLE_COUNT_2_BIT ) { return VK_SAMPLE_COUNT_2_BIT; }
		*/
		return VK_SAMPLE_COUNT_1_BIT;
	}

	void RecreateSwapChain()
	{
		int width = 0, height = 0;
		gWindow.GetWindowFrameBufferSize( width, height, true );

		vkDeviceWaitIdle( context.device );

		DestroyFrameResources();
		swapChain.Destroy();
		frameBufferMemory.Reset();
		swapChain.Create( &gWindow, width, height );
		CreateRenderPasses();
		CreateFramebuffers();
	}

	VkFormat FindColorFormat()
	{
		return FindSupportedFormat(
			{ VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
		);
	}

	VkFormat FindDepthFormat()
	{
		return FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	void AllocateDeviceMemory( const uint32_t allocSize, const uint32_t typeIndex, AllocatorVkMemory& outAllocation )
	{
		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = allocSize;
		allocInfo.memoryTypeIndex = typeIndex;

		VkDeviceMemory memory;
		if ( vkAllocateMemory( context.device, &allocInfo, nullptr, &memory ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate buffer memory!" );
		}

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );
		VkMemoryType type = memProperties.memoryTypes[ typeIndex ];

		void* memPtr = nullptr;
		if ( ( type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) == 0 ) {
			if ( vkMapMemory( context.device, memory, 0, VK_WHOLE_SIZE, 0, &memPtr ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to map memory to context.device memory!" );
			}
		}

		outAllocation.Bind( memory, memPtr, allocSize, typeIndex );
	}

	// Init/Shutdown
	void						InitVulkan();
	void						InitImGui();
	void						ShutdownImGui();
	void						Cleanup();

	// Image Functions
	void						TransitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, const textureInfo_t& info );
	void						CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, VkImage& image, const uint32_t width, const uint32_t height, const uint32_t layers );
	void						GenerateMipmaps( VkImage image, VkFormat imageFormat, const textureInfo_t& info );

	void						CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );

	// API Creation Functions
	void						CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, GpuBuffer& buffer, AllocatorVkMemory& bufferMemory );
	void						CreateImage( const textureInfo_t& info, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits numSamples, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, GpuImage& image, AllocatorVkMemory& memory );
	void						CreateDescriptorSets( VkDescriptorSetLayout& layout, VkDescriptorSet descSets[ MAX_FRAMES_STATES ] );
	void						CreateDescSetLayouts();
	void						CreateDescriptorPool();
	void						CreateInstance();
	void						CreateResourceBuffers();
	void						CreateLogicalDevice();
	void						CreateSyncObjects();
	void						CreateCommandBuffers();
	void						CreateTextureSamplers();
	void						CreateUniformBuffers();
	void						CreateRenderPasses();
	void						CreateFramebuffers();
	void						CreateCommandPools();

	// Draw Frame
	void						Render( RenderView& view );
	void						Commit( const Scene* scene );
	void						CommitModel( RenderView& view, const Entity& ent, const uint32_t objectOffset );
	void						MergeSurfaces( RenderView& view );
	gfxStateBits_t				GetStateBitsForDrawPass( const drawPass_t pass );
	viewport_t					GetDrawPassViewport( const drawPass_t pass );
	void						DrawDebugMenu();
	void						WaitForEndFrame();
	void						SubmitFrame();

	// Misc
	void						DestroyFrameResources();
	void						CreateCodeTextures();
	VkCommandBuffer				BeginSingleTimeCommands();
	void						EndSingleTimeCommands( VkCommandBuffer commandBuffer );
	void						PickPhysicalDevice();

	// Update
	void						UpdateView();
	void						UploadTextures();
	void						UpdateGpuMaterials();
	void						UploadModelsToGPU();
	void						UpdateBufferContents( uint32_t currentImage );
	void						UpdateFrameDescSet( const int currentImage );
	void						UpdateDescriptorSets();

	// Debug
	static VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData )
	{

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	std::vector<const char*>	GetRequiredExtensions() const;	
	bool						CheckValidationLayerSupport();
	void						PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo );	
	void						SetupDebugMessenger();
	static VkResult				CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
	static void					DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator );
};