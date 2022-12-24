#pragma once

#include <camera.h>

#include "common.h"
#include "window.h"
#include "common.h"
#include "render_util.h"
#include "deviceContext.h"
#include "scene.h"
#include "pipeline.h"
#include "swapChain.h"
#include "assetLib.h"
#include "FrameState.h"
#include "renderConstants.h"

#include "io.h"
#include "GeoBuilder.h"

#if defined( USE_IMGUI )
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#endif

#if defined( USE_IMGUI )
static ImGui_ImplVulkanH_Window imguiMainWindowData;
extern imguiControls_t imguiControls;
#endif

typedef AssetLib< pipelineObject_t> AssetLibPipelines;

extern AssetLibPipelines			pipelineLib;
extern Scene						scene;
extern Window						window;

class Renderer
{
public:
	void Init()
	{
		InitVulkan();
		InitImGui();
	}

	void RenderScene( Scene& scene )
	{
		UpdateGpuMaterials();
		Commit( scene );
		SubmitFrame();

		localMemory.Pack();
		sharedMemory.Pack();
	}

	void Destroy()
	{
		vkDeviceWaitIdle( context.device );
		Cleanup();
	}

	bool IsReady() {
		return ( frameNumber > 0 );
	}

	// TODO: move this function
	static void GenerateGpuPrograms( AssetLibGpuProgram& lib )
	{
		const uint32_t programCount = lib.Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram* prog = lib.Find( i );
			for ( int i = 0; i < GpuProgram::MaxShaders; ++i ) {
				prog->vk_shaders[ i ] = CreateShaderModule( prog->shaders[ i ].blob );
			}
		}
	}

	// TODO: move
	void CreatePipelineObjects()
	{
		pipelineLib.Clear();
		for ( uint32_t i = 0; i < scene.materialLib.Count(); ++i )
		{
			const Material* m = scene.materialLib.Find( i );

			for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
				GpuProgram* prog = scene.gpuPrograms.Find( m->shaders[ passIx ] );
				if ( prog == nullptr ) {
					continue;
				}

				pipelineState_t state;
				state.viewport = GetDrawPassViewport( (drawPass_t)passIx );
				state.stateBits = GetStateBitsForDrawPass( (drawPass_t)passIx );
				state.shaders = prog;
				state.tag = scene.gpuPrograms.FindName( m->shaders[ passIx ] );

				VkRenderPass pass;
				VkDescriptorSetLayout layout;
				if ( passIx == DRAWPASS_SHADOW ) {
					pass = shadowPassState.pass;
					layout = globalLayout;
				}
				else if ( passIx == DRAWPASS_POST_2D ) {
					pass = postPassState.pass;
					layout = postProcessLayout;
				}
				else {
					pass = mainPassState.pass;
					layout = globalLayout;
				}

				CreateGraphicsPipeline( layout, pass, state, prog->pipeline );
			}
		}
	}

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

	std::vector< materialBufferObject_t > materialBuffer;

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
	float							renderTime = 0.0f;
	GpuBuffer						stagingBuffer;
	GpuBuffer						vb;	// move
	GpuBuffer						ib;

	AllocatorVkMemory				localMemory;
	AllocatorVkMemory				frameBufferMemory;
	AllocatorVkMemory				sharedMemory;

	VkSampler						vk_bilinearSampler;
	VkSampler						vk_depthShadowSampler;

	float							nearPlane = 1000.0f;
	float							farPlane = 0.1f;

	void CreateInstance()
	{
		if ( enableValidationLayers && !CheckValidationLayerSupport() )
		{
			throw std::runtime_error( "validation layers requested, but not available!" );
		}

		VkApplicationInfo appInfo{ };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
		std::vector<VkExtensionProperties> extensionProperties( extensionCount );
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensionProperties.data() );

		std::cout << "available extensions:\n";

		for ( const auto& extension : extensionProperties )
		{
			std::cout << '\t' << extension.extensionName << '\n';
		}

		uint32_t glfwExtensionCount = 0;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if ( enableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebugMessengerCreateInfo( debugCreateInfo );
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		if ( vkCreateInstance( &createInfo, nullptr, &context.instance ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create instance!" );
		}
	}

	void UploadModelsToGPU();

	void CommitModel( RenderView& view, const Entity& ent, const uint32_t objectOffset );

	void MergeSurfaces( RenderView& view );

	void InitVulkan()
	{
		{
			// Device Set-up
			CreateInstance();
			SetupDebugMessenger();
			window.CreateSurface();
			PickPhysicalDevice();
			CreateLogicalDevice();
			swapChain.Create( &window, DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT );
		}

		{
			// Passes
			CreateRenderPasses();
		}

		{
			// Pool Creation
			CreateDescriptorPool();
			CreateCommandPools();
		}

		{
			// Memory Allocations
			uint32_t type = FindMemoryType( ~0x00, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
			AllocateDeviceMemory( MaxSharedMemory, type, sharedMemory );
			type = FindMemoryType( ~0x00, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			AllocateDeviceMemory( MaxLocalMemory, type, localMemory );
			AllocateDeviceMemory( MaxFrameBufferMemory, type, frameBufferMemory );
		}

		CreateResourceBuffers();
		CreateTextureSamplers();

		GenerateGpuPrograms( scene.gpuPrograms );

		CreateCodeTextures();
		CreateDescSetLayouts();
		CreatePipelineObjects();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

		{
			// Create Frame Resources
			CreateSyncObjects();
			CreateFramebuffers();
			CreateUniformBuffers();
			CreateCommandBuffers();
		}

		UploadTextures();
		UploadModelsToGPU();
		UpdateDescriptorSets();
	}

	void InitImGui()
	{
#if defined( USE_IMGUI )
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan( window.window, true );

		ImGui_ImplVulkan_InitInfo vkInfo = {};
		vkInfo.Instance = context.instance;
		vkInfo.PhysicalDevice = context.physicalDevice;
		vkInfo.Device = context.device;
		vkInfo.QueueFamily = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		vkInfo.Queue = context.graphicsQueue;
		vkInfo.PipelineCache = nullptr;
		vkInfo.DescriptorPool = descriptorPool;
		vkInfo.Allocator = nullptr;
		vkInfo.MinImageCount = 2;
		vkInfo.ImageCount = 2;
		vkInfo.CheckVkResultFn = nullptr;
		ImGui_ImplVulkan_Init( &vkInfo, postPassState.pass );
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Upload Fonts
		{
			vkResetCommandPool( context.device, graphicsQueue.commandPool, 0 );
			VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
			ImGui_ImplVulkan_CreateFontsTexture( commandBuffer );
			EndSingleTimeCommands( commandBuffer );
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
		ImGui_ImplGlfw_NewFrame();
#endif
		imguiControls.heightMapHeight = 1.0f;
		imguiControls.roughness = 0.9f;
		imguiControls.shadowStrength = 0.99f;
		imguiControls.toneMapColor[ 0 ] = 1.0f;
		imguiControls.toneMapColor[ 1 ] = 1.0f;
		imguiControls.toneMapColor[ 2 ] = 1.0f;
		imguiControls.toneMapColor[ 3 ] = 1.0f;
		imguiControls.dbgImageId = -1;
		imguiControls.selectedEntityId = -1;
		imguiControls.selectedModelOrigin = vec3f( 0.0f );
	}

	void ShutdownImGui()
	{
#if defined( USE_IMGUI )
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
#endif
	}

	void CreateDescSetLayouts()
	{
		CreateSceneRenderDescriptorSetLayout( globalLayout );
		CreateSceneRenderDescriptorSetLayout( postProcessLayout );
		CreateDescriptorSets( globalLayout, mainPassState.descriptorSets );
		CreateDescriptorSets( globalLayout, shadowPassState.descriptorSets );
		CreateDescriptorSets( postProcessLayout, postPassState.descriptorSets );
	}

	gfxStateBits_t GetStateBitsForDrawPass( const drawPass_t pass );

	viewport_t GetDrawPassViewport( const drawPass_t pass );

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

	void PickPhysicalDevice();

	void CreateLogicalDevice();

	void DestroyFrameResources()
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

	void RecreateSwapChain()
	{
		int width = 0, height = 0;
		window.GetWindowFrameBufferSize( width, height, true );

		vkDeviceWaitIdle( context.device );

		DestroyFrameResources();
		swapChain.Destroy();
		frameBufferMemory.Reset();
		swapChain.Create( &window, width, height );
		CreateRenderPasses();
		CreateFramebuffers();
	}

	void CreateDescriptorSets( VkDescriptorSetLayout& layout, VkDescriptorSet descSets[ MAX_FRAMES_STATES ] )
	{
		std::vector<VkDescriptorSetLayout> layouts( swapChain.GetBufferCount(), layout );
		VkDescriptorSetAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = MAX_FRAMES_STATES;
		allocInfo.pSetLayouts = layouts.data();

		if ( vkAllocateDescriptorSets( context.device, &allocInfo, descSets ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate descriptor sets!" );
		}
	}

	void UpdateFrameDescSet( const int currentImage );

	void UpdateDescriptorSets()
	{
		const uint32_t frameBufferCount = static_cast<uint32_t>( swapChain.GetBufferCount() );
		for ( size_t i = 0; i < frameBufferCount; i++ )
		{
			UpdateFrameDescSet( static_cast<uint32_t>( i ) );
		}
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

	void CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, GpuBuffer& buffer, AllocatorVkMemory& bufferMemory )
	{
		VkBufferCreateInfo bufferInfo{ };
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if ( vkCreateBuffer( context.device, &bufferInfo, nullptr, &buffer.GetVkObject() ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create buffer!" );
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements( context.device, buffer.GetVkObject(), &memRequirements );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( memRequirements.memoryTypeBits, properties );

		if ( bufferMemory.Allocate( memRequirements.alignment, memRequirements.size, buffer.alloc ) ) {
			vkBindBufferMemory( context.device, buffer.GetVkObject(), bufferMemory.GetMemoryResource(), buffer.alloc.GetOffset() );
		} else {
			throw std::runtime_error( "Buffer could not allocate!" );
		}
	}

	void CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );

	void CreateTextureSamplers();

	void CreateCodeTextures() {
		textureInfo_t info{};
		info.width = ShadowMapWidth;
		info.height = ShadowMapHeight;
		info.mipLevels = 1;
		info.layers = 1;

		// Default Images
		CreateImage( info, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, msaaSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rc.whiteImage, localMemory );
		rc.whiteImage.vk_view = CreateImageView( rc.whiteImage.vk_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

		CreateImage( info, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, msaaSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rc.blackImage, localMemory );
		rc.blackImage.vk_view = CreateImageView( rc.blackImage.vk_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1 );
	}

	void CreateUniformBuffers()
	{
		for ( size_t i = 0; i < swapChain.GetBufferCount(); ++i )
		{
			// Globals Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( globalUboConstants_t ) );
				const VkDeviceSize bufferSize = stride;
				CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].globalConstants, sharedMemory );
			}

			// Model Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( uniformBufferObject_t ) );
				const VkDeviceSize bufferSize = MaxSurfaces * stride;
				CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].surfParms, sharedMemory );
			}

			// Material Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( materialBufferObject_t ) );
				const VkDeviceSize materialBufferSize = MaxMaterialDescriptors * stride;
				CreateBuffer( materialBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].materialBuffers, sharedMemory );
			}

			// Light Buffer
			{
				const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( light_t ) );
				const VkDeviceSize lightBufferSize = MaxLights * stride;
				CreateBuffer( lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].lightParms, sharedMemory );
			}
		}
	}

	void CreateRenderPasses()
	{
		{
			// Main View Pass
			VkAttachmentDescription colorAttachment{ };
			colorAttachment.format = FindColorFormat();
			colorAttachment.samples = msaaSamples;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorAttachmentRef{ };
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription depthAttachment{ };
			depthAttachment.format = FindDepthFormat();
			depthAttachment.samples = msaaSamples;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

			VkAttachmentReference depthAttachmentRef{ };
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription stencilAttachment{ };
			stencilAttachment.format = FindDepthFormat();
			stencilAttachment.samples = msaaSamples;
			stencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			stencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			stencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			stencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			stencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			stencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

			VkAttachmentReference stencilAttachmentRef{ };
			stencilAttachmentRef.attachment = 1;
			stencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{ };
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			std::array<VkSubpassDependency, 2> dependencies;
			dependencies[ 0 ].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[ 0 ].dstSubpass = 0;
			dependencies[ 0 ].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[ 0 ].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[ 0 ].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[ 0 ].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[ 0 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[ 1 ].srcSubpass = 0;
			dependencies[ 1 ].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[ 1 ].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[ 1 ].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[ 1 ].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[ 1 ].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[ 1 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, stencilAttachment };
			VkRenderPassCreateInfo renderPassInfo{ };
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = static_cast<uint32_t>( dependencies.size() );
			renderPassInfo.pDependencies = dependencies.data();

			mainPassState.pass = NULL;
			if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &mainPassState.pass ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create render pass!" );
			}
		}

		{
			// Shadow Pass
			VkAttachmentDescription colorAttachment{ };
			colorAttachment.format = swapChain.GetBackBufferFormat();
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{ };
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription depthAttachment{ };
			depthAttachment.format = VK_FORMAT_D32_SFLOAT;
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

			VkAttachmentReference depthAttachmentRef{ };
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{ };
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[ 0 ].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[ 0 ].dstSubpass = 0;
			dependencies[ 0 ].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[ 0 ].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependencies[ 0 ].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[ 0 ].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[ 0 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[ 1 ].srcSubpass = 0;
			dependencies[ 1 ].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[ 1 ].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[ 1 ].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[ 1 ].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[ 1 ].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[ 1 ].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
			VkRenderPassCreateInfo renderPassInfo{ };
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = static_cast<uint32_t>( dependencies.size() );
			renderPassInfo.pDependencies = dependencies.data();

			shadowPassState.pass = NULL;
			if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &shadowPassState.pass ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create render pass!" );
			}
		}

		{
			// Post-Process Pass
			VkAttachmentDescription colorAttachment{ };
			colorAttachment.format = swapChain.GetBackBufferFormat();
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{ };
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{ };
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = nullptr;

			VkSubpassDependency dependency{ };
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			std::array<VkAttachmentDescription, 1> attachments = { colorAttachment };
			VkRenderPassCreateInfo renderPassInfo{ };
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			postPassState.pass = NULL;
			if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &postPassState.pass ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create render pass!" );
			}
		}
	}

	void CreateShadowPass( VkRenderPass& pass )
	{
		
	}

	void CreatePostProcessPass( VkRenderPass& pass )
	{

	}

	void CreateFramebuffers()
	{
		int width = 0;
		int height = 0;
		window.GetWindowFrameBufferSize( width, height );

		/////////////////////////////////
		//       Shadow Map Render     //
		/////////////////////////////////
		for ( size_t i = 0; i < MAX_FRAMES_STATES; ++i )
		{
			textureInfo_t info{};
			info.width = ShadowMapWidth;
			info.height = ShadowMapHeight;
			info.mipLevels = 1;
			info.layers = 1;
			CreateImage( info, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, msaaSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].shadowMapImage, frameBufferMemory );
			frameState[ i ].shadowMapImage.vk_view = CreateImageView( frameState[ i ].shadowMapImage.vk_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 1 );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
		{
			std::array<VkImageView, 2> attachments = {
				rc.whiteImage.vk_view,
				frameState[ i ].shadowMapImage.vk_view,
			};

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = shadowPassState.pass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = ShadowMapWidth;
			framebufferInfo.height = ShadowMapHeight;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &shadowPassState.fb[ i ] ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create framebuffer!" );
			}
		}

		/////////////////////////////////
		//     Main Scene 3D Render    //
		/////////////////////////////////
		for ( size_t i = 0; i < swapChain.GetBufferCount(); i++ )
		{
			textureInfo_t info{};
			info.width = width;
			info.height = height;
			info.mipLevels = 1;
			info.layers = 1;

			VkFormat colorFormat = FindColorFormat();

			CreateImage( info, colorFormat, VK_IMAGE_TILING_OPTIMAL, msaaSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].viewColorImage, frameBufferMemory );
			frameState[ i ].viewColorImage.vk_view = CreateImageView( frameState[ i ].viewColorImage.vk_image, colorFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

			VkFormat depthFormat = FindDepthFormat();
			CreateImage( info, depthFormat, VK_IMAGE_TILING_OPTIMAL, msaaSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].depthImage, frameBufferMemory );
			frameState[ i ].depthImage.vk_view = CreateImageView( frameState[ i ].depthImage.vk_image, depthFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 1 );
			frameState[ i ].stencilImage.vk_view = CreateImageView( frameState[ i ].depthImage.vk_image, depthFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_STENCIL_BIT, 1 );

			std::array<VkImageView, 3> attachments = {
				frameState[ i ].viewColorImage.vk_view,
				frameState[ i ].depthImage.vk_view,
				frameState[ i ].stencilImage.vk_view,
			};

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mainPassState.pass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = width;
			framebufferInfo.height = height;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &mainPassState.fb[ i ] ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create scene framebuffer!" );
			}
		}
		/////////////////////////////////
		//       Swap Chain Images     //
		/////////////////////////////////
		swapChain.vk_framebuffers.resize( swapChain.GetBufferCount() );
		for ( size_t i = 0; i < swapChain.GetBufferCount(); i++ )
		{
			std::array<VkImageView, 1> attachments = {
				swapChain.vk_swapChainImageViews[ i ]
			};

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = postPassState.pass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChain.vk_swapChainExtent.width;
			framebufferInfo.height = swapChain.vk_swapChainExtent.height;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &swapChain.vk_framebuffers[ i ] ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create swap chain framebuffer!" );
			}
			postPassState.fb[ i ] = swapChain.vk_framebuffers[ i ];
		}
	}

	void CreateCommandPools()
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &graphicsQueue.commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create graphics command pool!" );
		}

		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_COMPUTE ];
		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &computeQueue.commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create compute command pool!" );
		}
	}

	void CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 3> poolSizes{ };
		poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[ 0 ].descriptorCount = DescriptorPoolMaxUniformBuffers;
		poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[ 1 ].descriptorCount = DescriptorPoolMaxComboImages;
		poolSizes[ 2 ].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSizes[ 2 ].descriptorCount = DescriptorPoolMaxImages;

		VkDescriptorPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>( poolSizes.size() );
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = DescriptorPoolMaxSets;

		if ( vkCreateDescriptorPool( context.device, &poolInfo, nullptr, &descriptorPool ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create descriptor pool!" );
		}
	}

	VkCommandBuffer BeginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = graphicsQueue.commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers( context.device, &allocInfo, &commandBuffer );

		VkCommandBufferBeginInfo beginInfo{ };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer( commandBuffer, &beginInfo );

		return commandBuffer;
	}

	void EndSingleTimeCommands( VkCommandBuffer commandBuffer )
	{
		vkEndCommandBuffer( commandBuffer );

		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit( context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
		vkQueueWaitIdle( context.graphicsQueue );

		vkFreeCommandBuffers( context.device, graphicsQueue.commandPool, 1, &commandBuffer );
	}

	void TransitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, const textureInfo_t& info )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{ };
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = info.mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = info.layers;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument( "unsupported layout transition!" );
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands( commandBuffer );
	}

	void CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, VkImage& image, const uint32_t width, const uint32_t height, const uint32_t layers );

	void CreateCommandBuffers()
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		// Graphics
		{
			allocInfo.commandPool = graphicsQueue.commandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

			if ( vkAllocateCommandBuffers( context.device, &allocInfo, graphicsQueue.commandBuffers ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to allocate graphics command buffers!" );
			}

			for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
				vkResetCommandBuffer( graphicsQueue.commandBuffers[ i ], 0 );
			}
		}

		// Compute
		{
			allocInfo.commandPool = computeQueue.commandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;

			if ( vkAllocateCommandBuffers( context.device, &allocInfo, &computeQueue.commandBuffer ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to allocate compute command buffers!" );
			}
			vkResetCommandBuffer( computeQueue.commandBuffer, 0 );
		}
	}

	void Render( RenderView& view )
	{
		const uint32_t i = bufferId;
		vkResetCommandBuffer( graphicsQueue.commandBuffers[ i ], 0 );

		VkCommandBufferBeginInfo beginInfo{ };
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		int width = 0;
		int height = 0;
		window.GetWindowSize( width, height );

		if ( vkBeginCommandBuffer( graphicsQueue.commandBuffers[ i ], &beginInfo ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to begin recording command buffer!" );
		}

		VkBuffer vertexBuffers[] = { vb.GetVkObject() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers( graphicsQueue.commandBuffers[ i ], 0, 1, vertexBuffers, offsets );
		vkCmdBindIndexBuffer( graphicsQueue.commandBuffers[ i ], ib.GetVkObject(), 0, VK_INDEX_TYPE_UINT32 );

		// Shadow Passes
		{
			VkRenderPassBeginInfo shadowPassInfo{ };
			shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			shadowPassInfo.renderPass = shadowPassState.pass;
			shadowPassInfo.framebuffer = shadowPassState.fb[ i ];
			shadowPassInfo.renderArea.offset = { 0, 0 };
			shadowPassInfo.renderArea.extent = { ShadowMapWidth, ShadowMapHeight };

			std::array<VkClearValue, 2> shadowClearValues{ };
			shadowClearValues[ 0 ].color = { 1.0f, 1.0f, 1.0f, 1.0f };
			shadowClearValues[ 1 ].depthStencil = { 1.0f, 0 };

			shadowPassInfo.clearValueCount = static_cast<uint32_t>( shadowClearValues.size() );
			shadowPassInfo.pClearValues = shadowClearValues.data();
	
			//VkDebugUtilsLabelEXT markerInfo = {};
			//markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			//markerInfo.pLabelName = "Begin Section";
			//vkCmdBeginDebugUtilsLabelEXT( graphicsQueue.commandBuffers[ i ], &markerInfo );

			//// contents of section here

			//vkCmdEndDebugUtilsLabelEXT( graphicsQueue.commandBuffers[ i ] );

			// TODO: how to handle views better?
			vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE );

			for ( size_t surfIx = 0; surfIx < shadowView.mergedModelCnt; surfIx++ )
			{
				drawSurf_t& surface = shadowView.merged[ surfIx ];

				pipelineObject_t* pipelineObject;
				if ( surface.pipelineObject[ DRAWPASS_SHADOW ] == INVALID_HDL ) {
					continue;
				}
				if ( ( surface.flags & SKIP_OPAQUE ) != 0 ) {
					continue;
				}
				GetPipelineObject( surface.pipelineObject[ DRAWPASS_SHADOW ], &pipelineObject );

				// vkCmdSetDepthBias
				vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
				vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &shadowPassState.descriptorSets[ i ], 0, nullptr );

				VkViewport viewport{ };
				viewport.x = shadowView.viewport.x;
				viewport.y = shadowView.viewport.y;
				viewport.width = shadowView.viewport.width;
				viewport.height = shadowView.viewport.height;
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
				vkCmdSetViewport( graphicsQueue.commandBuffers[ i ], 0, 1, &viewport );

				VkRect2D rect{ };
				rect.extent.width = static_cast< uint32_t >( shadowView.viewport.width );
				rect.extent.height = static_cast<uint32_t>( shadowView.viewport.height );
				vkCmdSetScissor( graphicsQueue.commandBuffers[ i ], 0, 1, &rect );

				pushConstants_t pushConstants = { surface.objectId, surface.materialId };
				vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

				vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, shadowView.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
			}
			vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
		}

		// Main Passes
		{
			VkRenderPassBeginInfo renderPassInfo{ };
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = mainPassState.pass;
			renderPassInfo.framebuffer = mainPassState.fb[ i ];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChain.vk_swapChainExtent;

			std::array<VkClearValue, 2> clearValues{ };
			clearValues[ 0 ].color = { 0.0f, 0.1f, 0.5f, 1.0f };
			clearValues[ 1 ].depthStencil = { 0.0f, 0x00 };

			renderPassInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
			renderPassInfo.pClearValues = clearValues.data();
			renderPassInfo.clearValueCount = 2;

			vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

			VkViewport viewport{ };
			viewport.x = view.viewport.x;
			viewport.y = view.viewport.y;
			viewport.width = view.viewport.width;
			viewport.height = view.viewport.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport( graphicsQueue.commandBuffers[ i ], 0, 1, &viewport );

			VkRect2D rect{ };
			rect.extent.width = static_cast<uint32_t>( view.viewport.width );
			rect.extent.height = static_cast<uint32_t>( view.viewport.height );
			vkCmdSetScissor( graphicsQueue.commandBuffers[ i ], 0, 1, &rect );

			for ( uint32_t pass = DRAWPASS_DEPTH; pass < DRAWPASS_POST_2D; ++pass )
			{
				for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
				{
					drawSurf_t& surface = view.merged[ surfIx ];

					pipelineObject_t* pipelineObject;
					if ( surface.pipelineObject[ pass ] == INVALID_HDL ) {
						continue;
					}
					if ( ( pass == DRAWPASS_OPAQUE ) && ( ( surface.flags & SKIP_OPAQUE ) != 0 ) ) {
						continue;
					}
					if ( ( pass == DRAWPASS_DEBUG_WIREFRAME ) && ( ( surface.flags & WIREFRAME ) == 0 ) ) {
						continue;
					}
					if ( ( pass == DRAWPASS_DEBUG_SOLID ) && ( ( surface.flags & DEBUG_SOLID ) == 0 ) ) {
						continue;
					}
					GetPipelineObject( surface.pipelineObject[ pass ], &pipelineObject );

					if ( pass == DRAWPASS_DEPTH ) {
						vkCmdSetStencilReference( graphicsQueue.commandBuffers[ i ], VK_STENCIL_FACE_FRONT_BIT, surface.stencilBit );
					}

					vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &mainPassState.descriptorSets[ i ], 0, nullptr );

					pushConstants_t pushConstants = { surface.objectId, surface.materialId };
					vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

					vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, view.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
				}
			}
			vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
		}

		// Post Process Passes
		{
			VkRenderPassBeginInfo postProcessPassInfo{ };
			postProcessPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			postProcessPassInfo.renderPass = postPassState.pass;
			postProcessPassInfo.framebuffer = postPassState.fb[ i ];
			postProcessPassInfo.renderArea.offset = { 0, 0 };
			postProcessPassInfo.renderArea.extent = swapChain.vk_swapChainExtent;

			std::array<VkClearValue, 2> postProcessClearValues{ };
			postProcessClearValues[ 0 ].color = { 0.0f, 0.1f, 0.5f, 1.0f };
			postProcessClearValues[ 1 ].depthStencil = { 0.0f, 0 };

			postProcessPassInfo.clearValueCount = static_cast<uint32_t>( postProcessClearValues.size() );
			postProcessPassInfo.pClearValues = postProcessClearValues.data();

			vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &postProcessPassInfo, VK_SUBPASS_CONTENTS_INLINE );
			for ( size_t surfIx = 0; surfIx < view.mergedModelCnt; surfIx++ )
			{
				drawSurf_t& surface = shadowView.merged[ surfIx ];
				if ( surface.pipelineObject[ DRAWPASS_POST_2D ] == INVALID_HDL ) {
					continue;
				}
				pipelineObject_t* pipelineObject;
				GetPipelineObject( surface.pipelineObject[ DRAWPASS_POST_2D ], &pipelineObject );
				vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
				vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &postPassState.descriptorSets[ i ], 0, nullptr );

				pushConstants_t pushConstants = { surface.objectId, surface.materialId };
				vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

				vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, view.instanceCounts[ surfIx ], surface.firstIndex, surface.vertexOffset, 0 );
			}

#if defined( USE_IMGUI )
			ImGui_ImplVulkan_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin( "Control Panel" );
			ImGui::Checkbox( "Reload Shaders", &imguiControls.rebuildShaders );
			ImGui::InputFloat( "Heightmap Height", &imguiControls.heightMapHeight, 0.1f, 1.0f );
			ImGui::SliderFloat( "Roughness", &imguiControls.roughness, 0.1f, 1.0f );
			ImGui::SliderFloat( "Shadow Strength", &imguiControls.shadowStrength, 0.0f, 1.0f );
			ImGui::InputFloat( "Tone Map R", &imguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map G", &imguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map B", &imguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map A", &imguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
			ImGui::InputInt( "Image Id", &imguiControls.dbgImageId );
			ImGui::Text( "Mouse: (%f, %f )", (float)window.input.GetMouse().x, (float)window.input.GetMouse().y );
			ImGui::Text( "Mouse Dt: (%f, %f )", (float)window.input.GetMouse().dx, (float)window.input.GetMouse().dy );
			const vec2f screenPoint = vec2f( (float)window.input.GetMouse().x, (float)window.input.GetMouse().y );
			const vec2f ndc = 2.0f * Multiply( screenPoint, vec2f( 1.0f / width, 1.0f / height ) ) - vec2f( 1.0f );
			
			char entityName[ 256 ];
			if ( imguiControls.selectedEntityId >= 0 ) {
				sprintf_s( entityName, "%i: %s", imguiControls.selectedEntityId, scene.modelLib.FindName( ( *scene.entities.Find( imguiControls.selectedEntityId ) )->modelHdl ) );
			} else {
				memset( &entityName[ 0 ], 0, 256 );
			}
			static vec3f tempOrigin;
			ImGui::Text( "NDC: (%f, %f )", (float)ndc[ 0 ], (float)ndc[ 1 ] );

			ImGui::InputFloat( "Selected Model X: ", &imguiControls.selectedModelOrigin[ 0 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Selected Model Y: ", &imguiControls.selectedModelOrigin[ 1 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Selected Model Z: ", &imguiControls.selectedModelOrigin[ 2 ], 0.1f, 1.0f );

			if ( imguiControls.selectedEntityId >= 0 ) {
				Entity* entity = scene.FindEntity( imguiControls.selectedEntityId );
				entity->SetOrigin( vec3f(	tempOrigin[ 0 ] + imguiControls.selectedModelOrigin[ 0 ],
											tempOrigin[ 1 ] + imguiControls.selectedModelOrigin[ 1 ],
											tempOrigin[ 2 ] + imguiControls.selectedModelOrigin[ 2 ] ) );
			}
			ImGui::Text( "Frame Number: %d", frameNumber );
			ImGui::SameLine();
			ImGui::Text( "FPS: %f", 1000.0f / renderTime );
			//ImGui::Text( "Model %i: %s", 0, models[ 0 ].name.c_str() );
			ImGui::End();

			// Render dear imgui into screen
			ImGui::Render();
			ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), graphicsQueue.commandBuffers[ i ] );
#endif
			vkCmdEndRenderPass( graphicsQueue.commandBuffers[ i ] );
		}


		if ( vkEndCommandBuffer( graphicsQueue.commandBuffers[ i ] ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to record command buffer!" );
		}
	}

	void CreateImage( const textureInfo_t& info, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits numSamples, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, GpuImage& image, AllocatorVkMemory& memory )
	{
		VkImageCreateInfo imageInfo{ };
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>( info.width );
		imageInfo.extent.height = static_cast<uint32_t>( info.height );
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = info.mipLevels;
		imageInfo.arrayLayers = info.layers;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = numSamples;
		if ( format == FindDepthFormat() )
		{
			VkImageStencilUsageCreateInfo stencilUsage{}; 
			stencilUsage.sType = VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO;
			stencilUsage.stencilUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			imageInfo.pNext = &stencilUsage;
		} else {
			imageInfo.flags = ( info.type == TEXTURE_TYPE_CUBE ) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
		}

		if ( vkCreateImage( context.device, &imageInfo, nullptr, &image.vk_image ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create image!" );
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements( context.device, image.vk_image, &memRequirements );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( memRequirements.memoryTypeBits, properties );

		AllocationVk alloc;
		if ( memory.Allocate( memRequirements.alignment, memRequirements.size, alloc ) ) {
			vkBindImageMemory( context.device, image.vk_image, memory.GetMemoryResource(), alloc.GetOffset() );
		} else {
			throw std::runtime_error( "Buffer could not be allocated!" );
		}
	}

	void GenerateMipmaps( VkImage image, VkFormat imageFormat, const textureInfo_t& info )
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties( context.physicalDevice, imageFormat, &formatProperties );

		if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) )
		{
			throw std::runtime_error( "texture image format does not support linear blitting!" );
		}

		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{ };
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = info.layers;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = info.width;
		int32_t mipHeight = info.height;

		for ( uint32_t i = 1; i < info.mipLevels; i++ )
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier( commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier );

			VkImageBlit blit{ };
			blit.srcOffsets[ 0 ] = { 0, 0, 0 };
			blit.srcOffsets[ 1 ] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = info.layers;
			blit.dstOffsets[ 0 ] = { 0, 0, 0 };
			blit.dstOffsets[ 1 ] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = info.layers;

			vkCmdBlitImage( commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR );

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier( commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier );

			if ( mipWidth > 1 ) mipWidth /= 2;
			if ( mipHeight > 1 ) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = info.mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier( commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier );

		EndSingleTimeCommands( commandBuffer );
	}

	void CreateResourceBuffers()
	{
		stagingBuffer.Reset();
		const uint64_t size = 256 * MB_1;
		CreateBuffer( size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, sharedMemory );
	}

	void UploadTextures();
	void UpdateGpuMaterials();

	void CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{ };
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{ };
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
		{
			if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &graphicsQueue.imageAvailableSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &graphicsQueue.renderFinishedSemaphores[ i ] ) != VK_SUCCESS ||
				vkCreateFence( context.device, &fenceInfo, nullptr, &graphicsQueue.inFlightFences[ i ] ) != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create synchronization objects for a frame!" );
			}
		}

		if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &computeQueue.semaphore ) ) {
			throw std::runtime_error( "Failed to create compute semaphore!" );
		}
	}

	void UpdateView()
	{
		int width;
		int height;
		window.GetWindowSize( width, height );
		renderView.viewport.width = static_cast<float>( width );
		renderView.viewport.height = static_cast<float>( height );

		vec3f shadowLightDir;
		shadowLightDir[ 0 ] = -renderView.lights[ 0 ].lightDir[ 0 ];
		shadowLightDir[ 1 ] = -renderView.lights[ 0 ].lightDir[ 1 ];
		shadowLightDir[ 2 ] = -renderView.lights[ 0 ].lightDir[ 2 ];

		// Temp shadow map set-up
		shadowView.viewport.x = 0.0f;
		shadowView.viewport.y = 0.0f;
		shadowView.viewport.near = 0.0f;
		shadowView.viewport.far = 1.0f;
		shadowView.viewport.width = ShadowMapWidth;
		shadowView.viewport.height = ShadowMapHeight;
		shadowView.viewMatrix = MatrixFromVector( shadowLightDir );
		shadowView.viewMatrix = shadowView.viewMatrix.Transpose();
		const vec4f shadowLightPos = shadowView.viewMatrix * renderView.lights[ 0 ].lightPos;
		shadowView.viewMatrix[ 3 ][ 0 ] = -shadowLightPos[ 0 ];
		shadowView.viewMatrix[ 3 ][ 1 ] = -shadowLightPos[ 1 ];
		shadowView.viewMatrix[ 3 ][ 2 ] = -shadowLightPos[ 2 ];

		Camera shadowCam;
		shadowCam = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
		shadowCam.fov = Radians( 90.0f );
		shadowCam.far = nearPlane;
		shadowCam.near = farPlane;
		shadowCam.aspect = ( ShadowMapWidth / (float)ShadowMapHeight );
		shadowCam.halfFovX = tan( 0.5f * Radians( 90.0f ) );
		shadowCam.halfFovY = tan( 0.5f * Radians( 90.0f ) ) / shadowCam.aspect;
		shadowCam.focalLength = shadowCam.far;
		shadowCam.viewportWidth = 2.0f * shadowCam.halfFovX;
		shadowCam.viewportHeight = 2.0f * shadowCam.halfFovY;
		shadowView.projMatrix = shadowCam.GetPerspectiveMatrix();
	}

	void WaitForEndFrame()
	{
		vkWaitForFences( context.device, 1, &graphicsQueue.inFlightFences[ frameId ], VK_TRUE, UINT64_MAX );
	}

	void SubmitFrame()
	{
		WaitForEndFrame();

		VkResult result = vkAcquireNextImageKHR( context.device, swapChain.vk_swapChain, UINT64_MAX, graphicsQueue.imageAvailableSemaphores[ frameId ], VK_NULL_HANDLE, &bufferId );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR )
		{
			RecreateSwapChain();
			return;
		}
		else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
		{
			throw std::runtime_error( "Failed to acquire swap chain image!" );
		}

		// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		if ( graphicsQueue.imagesInFlight[ bufferId ] != VK_NULL_HANDLE ) {
			vkWaitForFences( context.device, 1, &graphicsQueue.imagesInFlight[ bufferId ], VK_TRUE, UINT64_MAX );
		}
		// Mark the image as now being in use by this frame
		graphicsQueue.imagesInFlight[ bufferId ] = graphicsQueue.inFlightFences[ frameId ];

		UpdateBufferContents( bufferId );
		UpdateFrameDescSet( bufferId );
		Render( renderView );

		VkSubmitInfo submitInfo{ };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { graphicsQueue.imageAvailableSemaphores[ frameId ] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphicsQueue.commandBuffers[ bufferId ];

		VkSemaphore signalSemaphores[] = { graphicsQueue.renderFinishedSemaphores[ frameId ] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences( context.device, 1, &graphicsQueue.inFlightFences[ frameId ] );

		if ( vkQueueSubmit( context.graphicsQueue, 1, &submitInfo, graphicsQueue.inFlightFences[ frameId ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to submit draw command buffer!" );
		}

		VkPresentInfoKHR presentInfo{ };
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain.GetApiObject() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &bufferId;
		presentInfo.pResults = nullptr; // Optional

		result = vkQueuePresentKHR( context.presentQueue, &presentInfo );

		if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.IsResizeRequested() )
		{
			RecreateSwapChain();
			window.AcceptImageResize();
			return;
		}
		else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR )
		{
			throw std::runtime_error( "Failed to acquire swap chain image!" );
		}

		frameId = ( frameId + 1 ) % MAX_FRAMES_IN_FLIGHT;
		++frameNumber;
		static auto prevTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		renderTime = std::chrono::duration<float, std::chrono::milliseconds::period>( currentTime - prevTime ).count();
		prevTime = currentTime;
	}

	void Cleanup()
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

		const uint32_t textureCount = scene.textureLib.Count();
		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			const texture_t* texture = scene.textureLib.Find( i );
			vkDestroyImageView( context.device, texture->image.vk_view, nullptr );
			vkDestroyImage( context.device, texture->image.vk_image, nullptr );
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

	void UpdateBufferContents( uint32_t currentImage );

	static VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData )
	{

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	void Commit( const Scene& scene );

	std::vector<const char*> GetRequiredExtensions() const;
	bool CheckValidationLayerSupport();
	void PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo );
	void SetupDebugMessenger();
	static VkResult CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
	static void DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator );
};