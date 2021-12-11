#pragma once

#include "common.h"
#include "window.h"
#include "common.h"
#include "util.h"
#include "deviceContext.h"
#include "camera.h"
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

typedef AssetLib< texture_t >		AssetLibImages;
typedef AssetLib< Material >		AssetLibMaterials;
typedef AssetLib< GpuProgram >		AssetLibGpuProgram;
typedef AssetLib< modelSource_t >	AssetLibModels;

extern AssetLibGpuProgram			gpuPrograms;
extern AssetLibMaterials			materialLib;
extern AssetLibImages				textureLib;
extern AssetLibModels				modelLib;
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

private:

	static const uint32_t				ShadowMapWidth = 1280;
	static const uint32_t				ShadowMapHeight = 720;

	static const bool					ValidateVerbose = false;
	static const bool					ValidateWarnings = false;
	static const bool					ValidateErrors = true;

	renderConstants_t					rc;
	std::vector<surfUpload_t>			modelUpload;
	RenderView							renderView;
	RenderView							shadowView;

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	VkDebugUtilsMessengerEXT		debugMessenger;
	SwapChain						swapChain;
	graphicsQueue_t					graphicsQueue;
	DrawPassState					shadowPassState;
	DrawPassState					mainPassState;
	DrawPassState					postPassState;
	VkDescriptorSetLayout			globalLayout;
	VkDescriptorSetLayout			postProcessLayout;
	VkDescriptorPool				descriptorPool;
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

	void UploadModels()
	{
		const VkDeviceSize vbSize = sizeof( VertexInput ) * MaxVertices;
		const VkDeviceSize ibSize = sizeof( uint32_t ) * MaxIndices;
		const uint32_t modelCount = modelLib.Count();
		modelUpload.resize( modelCount );
		CreateBuffer( vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vb, localMemory );
		CreateBuffer( ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ib, localMemory );

		static uint32_t vbBufElements = 0;
		static uint32_t ibBufElements = 0;
		for ( uint32_t m = 0; m < modelCount; ++m )
		{
			VkDeviceSize vbCopySize = sizeof( modelLib.Find( m )->vertices[ 0 ] ) * modelLib.Find( m )->vertices.size();
			VkDeviceSize ibCopySize = sizeof( modelLib.Find( m )->indices[ 0 ] ) * modelLib.Find( m )->indices.size();

			// VB Copy
			modelUpload[ m ].vertexOffset = vbBufElements;
			stagingBuffer.Reset();
			stagingBuffer.CopyData( modelLib.Find( m )->vertices.data(), static_cast<size_t>( vbCopySize ) );

			VkBufferCopy vbCopyRegion{ };
			vbCopyRegion.size = vbCopySize;
			vbCopyRegion.srcOffset = 0;
			vbCopyRegion.dstOffset = vb.GetSize();
			CopyGpuBuffer( stagingBuffer, vb, vbCopyRegion );

			vbBufElements += static_cast<uint32_t>( modelLib.Find( m )->vertices.size() );

			// IB Copy
			modelUpload[ m ].firstIndex = ibBufElements;

			stagingBuffer.Reset();
			stagingBuffer.CopyData( modelLib.Find( m )->indices.data(), static_cast<size_t>( ibCopySize ) );

			VkBufferCopy ibCopyRegion{ };
			ibCopyRegion.size = ibCopySize;
			ibCopyRegion.srcOffset = 0;
			ibCopyRegion.dstOffset = ib.GetSize();
			CopyGpuBuffer( stagingBuffer, ib, ibCopyRegion );

			ibBufElements += static_cast<uint32_t>( modelLib.Find( m )->indices.size() );
		}
	}

	void CommitModel( RenderView& view, const entity_t& model, const uint32_t objectOffset )
	{
		drawSurf_t& surf = view.surfaces[ view.committedModelCnt ];
		modelSource_t* source = modelLib.Find( model.modelId );
		surfUpload_t& upload = modelUpload[ model.modelId ];

		surf.vertexCount = upload.vertexCount;
		surf.vertexOffset = upload.vertexOffset;
		surf.firstIndex = upload.firstIndex;
		surf.indicesCnt = static_cast<uint32_t>( source->indices.size() );
		surf.materialId = model.materialId;
		surf.modelMatrix = model.matrix;
		surf.objectId = view.committedModelCnt + objectOffset;
		surf.flags = model.flags;

		for ( int i = 0; i < DRAWPASS_COUNT; ++i ) {
			if ( source->material->shaders[ i ].IsValid() ) {
				GpuProgram * prog = gpuPrograms.Find( source->material->shaders[ i ].Get() );
				if( prog == nullptr ) {
					continue;
				}
				surf.pipelineObject[ i ] = prog->pipeline;
			}
			else {
				surf.pipelineObject[ i ] = INVALID_HANDLE;
			}
		}

		++view.committedModelCnt;
	}

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
			CreateCommandPool();
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

		GenerateGpuPrograms();
		UploadTextures();
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

		UpdateDescriptorSets();
		UploadModels();
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
		imguiControls.toneMapColor[ 0 ] = 1.0f;
		imguiControls.toneMapColor[ 1 ] = 1.0f;
		imguiControls.toneMapColor[ 2 ] = 1.0f;
		imguiControls.toneMapColor[ 3 ] = 1.0f;
		imguiControls.selectedModelId = -1;
		imguiControls.selectedModelOrigin = glm::vec3( 0.0f );
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
		const uint32_t gpuProgramCount = gpuPrograms.Count();
		for ( uint32_t i = 0; i < gpuProgramCount; ++i )
		{
			GpuProgram* program = gpuPrograms.Find( i );
			CreateDescriptorSetLayout( *program );
		}

		CreateSceneRenderDescriptorSetLayout( globalLayout );
		CreateSceneRenderDescriptorSetLayout( postProcessLayout );
		CreateDescriptorSets( globalLayout, mainPassState.descriptorSets );
		CreateDescriptorSets( globalLayout, shadowPassState.descriptorSets );
		CreateDescriptorSets( postProcessLayout, postPassState.descriptorSets );
	}

	gfxStateBits_t GetStateBitsForDrawPass( const drawPass_t pass )
	{
		uint64_t stateBits = 0;
		if ( pass == DRAWPASS_SKYBOX )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_SHADOW )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			//	stateBits |= GFX_STATE_COLOR_MASK;
			stateBits |= GFX_STATE_DEPTH_OP_0; // FIXME: 3 bits, just set one for now
		//	stateBits |= GFX_STATE_CULL_MODE_FRONT;
		}
		else if ( pass == DRAWPASS_DEPTH )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_COLOR_MASK;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_TERRAIN )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_OPAQUE )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}
		else if ( pass == DRAWPASS_TRANS )
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
			stateBits |= GFX_STATE_BLEND_ENABLE;
		}
		else if ( pass == DRAWPASS_WIREFRAME )
		{
			stateBits |= GFX_STATE_WIREFRAME_ENABLE;
		}
		else if ( pass == DRAWPASS_POST_2D )
		{
			stateBits |= GFX_STATE_BLEND_ENABLE;
		}
		else
		{
			stateBits |= GFX_STATE_DEPTH_TEST;
			stateBits |= GFX_STATE_DEPTH_WRITE;
			stateBits |= GFX_STATE_CULL_MODE_BACK;
		}

		return static_cast<gfxStateBits_t>( stateBits );
	}

	viewport_t GetDrawPassViewport( const drawPass_t pass )
	{
		return renderView.viewport;
	}

	void CreatePipelineObjects()
	{
		for ( uint32_t i = 0; i < materialLib.Count(); ++i )
		{
			const Material* m = materialLib.Find( i );

			for ( int i = 0; i < DRAWPASS_COUNT; ++i ) {
				GpuProgram* prog = gpuPrograms.Find( m->shaders[ i ].Get() );
				if ( prog == nullptr ) {
					continue;
				}

				pipelineState_t state;
				state.viewport = GetDrawPassViewport( (drawPass_t)i );
				state.stateBits = GetStateBitsForDrawPass( (drawPass_t)i );
				state.shaders = prog;

				VkRenderPass pass;
				VkDescriptorSetLayout layout;
				if ( i == DRAWPASS_SHADOW ) {
					pass = shadowPassState.pass;
					layout = globalLayout;
				}
				else if ( i == DRAWPASS_POST_2D ) {
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

	void PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices( context.instance, &deviceCount, nullptr );

		if ( deviceCount == 0 ) {
			throw std::runtime_error( "Failed to find GPUs with Vulkan support!" );
		}

		std::vector<VkPhysicalDevice> devices( deviceCount );
		vkEnumeratePhysicalDevices( context.instance, &deviceCount, devices.data() );

		for ( const auto& device : devices )
		{
			if ( IsDeviceSuitable( device, window.vk_surface, deviceExtensions ) )
			{
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties( device, &deviceProperties );
				context.physicalDevice = device;
				context.limits = deviceProperties.limits;
				break;
			}
		}

		if ( context.physicalDevice == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to find a suitable GPU!" );
		}
	}

	void CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies( context.physicalDevice, window.vk_surface );
		context.queueFamilyIndices[ QUEUE_GRAPHICS ] = indices.graphicsFamily.value();
		context.queueFamilyIndices[ QUEUE_PRESENT ] = indices.presentFamily.value();
		context.queueFamilyIndices[ QUEUE_COMPUTE ] = indices.computeFamily.value();

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value() };

		float queuePriority = 1.0f;
		for ( uint32_t queueFamily : uniqueQueueFamilies )
		{
			VkDeviceQueueCreateInfo queueCreateInfo{ };
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		VkDeviceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>( queueCreateInfos.size() );
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		VkPhysicalDeviceFeatures deviceFeatures{ };
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.fillModeNonSolid = VK_TRUE;
		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>( deviceExtensions.size() );
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if ( enableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkPhysicalDeviceDescriptorIndexingFeatures descIndexing;
		memset( &descIndexing, 0, sizeof( VkPhysicalDeviceDescriptorIndexingFeatures ) );
		descIndexing.runtimeDescriptorArray = true;
		descIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descIndexing.pNext = NULL;
		createInfo.pNext = &descIndexing;

		if ( vkCreateDevice( context.physicalDevice, &createInfo, nullptr, &context.device ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create logical context.device!" );
		}

		vkGetDeviceQueue( context.device, indices.graphicsFamily.value(), 0, &context.graphicsQueue );
		vkGetDeviceQueue( context.device, indices.presentFamily.value(), 0, &context.presentQueue );
		vkGetDeviceQueue( context.device, indices.computeFamily.value(), 0, &context.computeQueue );
	}

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

		if ( vkAllocateDescriptorSets( context.device, &allocInfo, descSets ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to allocate descriptor sets!" );
		}
	}

	void UpdateFrameDescSet( const int currentImage )
	{
		const int i = currentImage;

		//////////////////////////////////////////////////////
		//													//
		// Scene Descriptor Sets							//
		//													//
		//////////////////////////////////////////////////////

		std::vector<VkDescriptorBufferInfo> globalConstantsInfo;
		globalConstantsInfo.reserve( 1 );
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].globalConstants.GetVkObject();
			info.offset = 0;
			info.range = sizeof( globalUboConstants_t );
			globalConstantsInfo.push_back( info );
		}

		std::vector<VkDescriptorBufferInfo> bufferInfo;
		bufferInfo.reserve( MaxSurfaces );
		for ( int j = 0; j < MaxSurfaces; ++j )
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].surfParms.GetVkObject();
			info.offset = j * sizeof( uniformBufferObject_t );
			info.range = sizeof( uniformBufferObject_t );
			bufferInfo.push_back( info );
		}

		std::vector<VkDescriptorImageInfo> imageInfo;
		imageInfo.reserve( MaxImageDescriptors );
		const uint32_t textureCount = textureLib.Count();
		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			texture_t* texture = textureLib.Find( i );
			VkImageView& imageView = texture->image.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			imageInfo.push_back( info );
		}
		// Defaults
		for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
		{
			const texture_t* texture = textureLib.GetDefault();
			const VkImageView& imageView = texture->image.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			imageInfo.push_back( info );
		}

		std::vector<VkDescriptorBufferInfo> materialBufferInfo;
		materialBufferInfo.reserve( MaxMaterialDescriptors );
		for ( int j = 0; j < MaxMaterialDescriptors; ++j )
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].materialBuffers.GetVkObject();
			info.offset = j * sizeof( materialBufferObject_t );
			info.range = sizeof( materialBufferObject_t );
			materialBufferInfo.push_back( info );
		}

		std::vector<VkDescriptorBufferInfo> lightBufferInfo;
		lightBufferInfo.reserve( MaxLights );
		for ( int j = 0; j < MaxLights; ++j )
		{
			VkDescriptorBufferInfo info{ };
			info.buffer = frameState[ i ].lightParms.GetVkObject();
			info.offset = j * sizeof( light_t );
			info.range = sizeof( light_t );
			lightBufferInfo.push_back( info );
		}

		std::vector<VkDescriptorImageInfo> codeImageInfo;
		codeImageInfo.reserve( MaxCodeImages );
		// Shadow Map
		{
			VkImageView& imageView = frameState[ currentImage ].shadowMapImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_depthShadowSampler;
			codeImageInfo.push_back( info );
		}

		const uint32_t descriptorSetCnt = 6;
		std::array<VkWriteDescriptorSet, descriptorSetCnt> descriptorWrites{ };

		uint32_t descriptorId = 0;
		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 0;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = 1;
		descriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 1;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
		descriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 2;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		descriptorWrites[ descriptorId ].pImageInfo = &imageInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 3;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
		descriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ]; // TODO: replace
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 4;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxLights;
		descriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
		++descriptorId;

		descriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[ descriptorId ].dstSet = mainPassState.descriptorSets[ i ];
		descriptorWrites[ descriptorId ].dstBinding = 5;
		descriptorWrites[ descriptorId ].dstArrayElement = 0;
		descriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		descriptorWrites[ descriptorId ].pImageInfo = &codeImageInfo[ 0 ];
		++descriptorId;

		assert( descriptorId == descriptorSetCnt );

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );

		//////////////////////////////////////////////////////
		//													//
		// Shadow Descriptor Sets							//
		//													//
		//////////////////////////////////////////////////////

		std::vector<VkDescriptorImageInfo> shadowImageInfo;
		shadowImageInfo.reserve( MaxImageDescriptors );
		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			texture_t* texture = textureLib.Find( i );
			VkImageView& imageView = texture->image.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			shadowImageInfo.push_back( info );
		}
		// Defaults
		for ( size_t j = textureCount; j < MaxImageDescriptors; ++j )
		{
			const texture_t* texture = textureLib.GetDefault();
			const VkImageView& imageView = texture->image.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			shadowImageInfo.push_back( info );
		}

		std::vector<VkDescriptorImageInfo> shadowCodeImageInfo;
		shadowCodeImageInfo.reserve( MaxCodeImages );
		for ( size_t j = 0; j < MaxCodeImages; ++j )
		{
			const texture_t* texture = textureLib.GetDefault();
			const VkImageView& imageView = texture->image.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			shadowCodeImageInfo.push_back( info );
		}

		std::array<VkWriteDescriptorSet, descriptorSetCnt> shadowDescriptorWrites{ };

		descriptorId = 0;
		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 0;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = 1;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 1;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 2;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowImageInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 3;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 4;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxLights;
		shadowDescriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
		++descriptorId;

		shadowDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowDescriptorWrites[ descriptorId ].dstSet = shadowPassState.descriptorSets[ i ];
		shadowDescriptorWrites[ descriptorId ].dstBinding = 5;
		shadowDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		shadowDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		shadowDescriptorWrites[ descriptorId ].pImageInfo = &shadowCodeImageInfo[ 0 ];
		++descriptorId;

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( shadowDescriptorWrites.size() ), shadowDescriptorWrites.data(), 0, nullptr );

		//////////////////////////////////////////////////////
		//													//
		// Post Descriptor Sets								//
		//													//
		//////////////////////////////////////////////////////
		std::vector<VkDescriptorImageInfo> postImageInfo;
		postImageInfo.reserve( 2 );
		// View Color Map
		{
			VkImageView& imageView = frameState[ currentImage ].viewColorImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_bilinearSampler;
			postImageInfo.push_back( info );
		}
		// View Depth Map
		{
			VkImageView& imageView = frameState[ currentImage ].depthImage.vk_view;
			VkDescriptorImageInfo info{ };
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			info.imageView = imageView;
			info.sampler = vk_depthShadowSampler;
			postImageInfo.push_back( info );
		}

		std::array<VkWriteDescriptorSet, 6> postDescriptorWrites{ };

		descriptorId = 0;
		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 0;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = 1;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &globalConstantsInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 1;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxSurfaces;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &bufferInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 2;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxImageDescriptors;
		postDescriptorWrites[ descriptorId ].pImageInfo = &imageInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 3;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxMaterialDescriptors;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &materialBufferInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 4;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxLights;
		postDescriptorWrites[ descriptorId ].pBufferInfo = &lightBufferInfo[ 0 ];
		++descriptorId;

		postDescriptorWrites[ descriptorId ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		postDescriptorWrites[ descriptorId ].dstSet = postPassState.descriptorSets[ i ];
		postDescriptorWrites[ descriptorId ].dstBinding = 5;
		postDescriptorWrites[ descriptorId ].dstArrayElement = 0;
		postDescriptorWrites[ descriptorId ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		postDescriptorWrites[ descriptorId ].descriptorCount = MaxCodeImages;
		postDescriptorWrites[ descriptorId ].pImageInfo = &postImageInfo[ 0 ];
		++descriptorId;

		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( postDescriptorWrites.size() ), postDescriptorWrites.data(), 0, nullptr );
	}

	void UpdateDescriptorSets()
	{
		const uint32_t frameBufferCount = static_cast<uint32_t>( swapChain.GetBufferCount() );
		for ( size_t i = 0; i < frameBufferCount; i++ )
		{
			UpdateFrameDescSet( static_cast<uint32_t>( i ) );
		}
	}

	uint32_t FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties )
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

		for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
		{
			if ( typeFilter & ( 1 << i ) && ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties )
			{
				return i;
			}
		}

		throw std::runtime_error( "Failed to find suitable memory type!" );
	}

	VkFormat FindDepthFormat()
	{
		return FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	VkFormat FindSupportedFormat( const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features )
	{
		for ( VkFormat format : candidates )
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties( context.physicalDevice, format, &props );

			if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features )
			{
				return format;
			}
			else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features )
			{
				return format;
			}
		}

		throw std::runtime_error( "Failed to find supported format!" );
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

	void CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
		vkCmdCopyBuffer( commandBuffer, srcBuffer.GetVkObject(), dstBuffer.GetVkObject(), 1, &copyRegion );
		EndSingleTimeCommands( commandBuffer );

		dstBuffer.Allocate( copyRegion.size );
	}

	void CreateTextureSamplers()
	{
		{
			// Default Bilinear Sampler
			VkSamplerCreateInfo samplerInfo{ };
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.maxAnisotropy = 16.0f;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 16.0f;
			samplerInfo.mipLodBias = 0.0f;

			if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &vk_bilinearSampler ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create texture sampler!" );
			}
		}

		{
			// Depth sampler
			VkSamplerCreateInfo samplerInfo{ };
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxAnisotropy = 0.0f;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 16.0f;
			samplerInfo.mipLodBias = 0.0f;

			if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &vk_depthShadowSampler ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create depth sampler!" );
			}
		}
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
			colorAttachment.format = swapChain.GetBackBufferFormat();
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
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

			std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
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
			depthAttachment.format = FindDepthFormat();
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
		const VkFormat depthFormat = FindDepthFormat();
		for ( size_t i = 0; i < MAX_FRAMES_STATES; ++i )
		{
			CreateImage( ShadowMapWidth, ShadowMapHeight, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].shadowMapImage, frameBufferMemory );
			frameState[ i ].shadowMapImage.vk_view = CreateImageView( frameState[ i ].shadowMapImage.vk_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1 );
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
			CreateImage( width, height, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].viewColorImage, frameBufferMemory );
			frameState[ i ].viewColorImage.vk_view = CreateImageView( frameState[ i ].viewColorImage.vk_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

			VkFormat depthFormat = FindDepthFormat();
			CreateImage( width, height, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameState[ i ].depthImage, frameBufferMemory );
			frameState[ i ].depthImage.vk_view = CreateImageView( frameState[ i ].depthImage.vk_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1 );

			std::array<VkImageView, 2> attachments = {
				frameState[ i ].viewColorImage.vk_view,
				frameState[ i ].depthImage.vk_view
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

	void CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &graphicsQueue.commandPool ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create command pool!" );
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

	void TransitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels )
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
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

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

	void CopyBufferToImage( VkCommandBuffer& commandBuffer, VkBuffer& buffer, const VkDeviceSize bufferOffset, VkImage& image, const uint32_t width, const uint32_t height )
	{
		VkBufferImageCopy region{ };
		memset( &region, 0, sizeof( region ) );
		region.bufferOffset = bufferOffset;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);
	}

	void CreateCommandBuffers()
	{
		VkCommandBufferAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = graphicsQueue.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, graphicsQueue.commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate command buffers!" );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( graphicsQueue.commandBuffers[ i ], 0 );
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

			// TODO: how to handle views better?
			vkCmdBeginRenderPass( graphicsQueue.commandBuffers[ i ], &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE );

			for ( size_t surfIx = 0; surfIx < shadowView.committedModelCnt; surfIx++ )
			{
				drawSurf_t& surface = shadowView.surfaces[ surfIx ];

				pipelineObject_t* pipelineObject;
				if ( surface.pipelineObject[ DRAWPASS_SHADOW ] == INVALID_HANDLE ) {
					continue;
				}
				GetPipelineObject( surface.pipelineObject[ DRAWPASS_SHADOW ], &pipelineObject );

				// vkCmdSetDepthBias
				vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
				vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &shadowPassState.descriptorSets[ i ], 0, nullptr );

				VkViewport viewport{ };
				viewport.x = view.viewport.x;
				viewport.y = view.viewport.y;
				viewport.width = view.viewport.width;
				viewport.height = view.viewport.height;
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
				vkCmdSetViewport( graphicsQueue.commandBuffers[ i ], 0, 1, &viewport );

				VkRect2D rect{ };
				rect.extent.width = static_cast< uint32_t >( view.viewport.width );
				rect.extent.height = static_cast<uint32_t>( view.viewport.height );
				vkCmdSetScissor( graphicsQueue.commandBuffers[ i ], 0, 1, &rect );

				pushConstants_t pushConstants = { surface.objectId, surface.materialId };
				vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

				vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, 1, surface.firstIndex, surface.vertexOffset, 0 );
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
			clearValues[ 1 ].depthStencil = { 0.0f, 0 };

			renderPassInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
			renderPassInfo.pClearValues = clearValues.data();

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
				for ( size_t surfIx = 0; surfIx < view.committedModelCnt; surfIx++ )
				{
					drawSurf_t& surface = view.surfaces[ surfIx ];

					pipelineObject_t* pipelineObject;
					if ( surface.pipelineObject[ pass ] == INVALID_HANDLE ) {
						continue;
					}
					if ( ( pass == DRAWPASS_WIREFRAME ) && ( ( surface.flags & WIREFRAME ) == 0 ) ) {
						continue;
					}
					GetPipelineObject( surface.pipelineObject[ pass ], &pipelineObject );

					vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
					vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &mainPassState.descriptorSets[ i ], 0, nullptr );

					pushConstants_t pushConstants = { surface.objectId, surface.materialId };
					vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

					vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, 1, surface.firstIndex, surface.vertexOffset, 0 );
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
			for ( size_t surfIx = 0; surfIx < view.committedModelCnt; surfIx++ )
			{
				drawSurf_t& surface = shadowView.surfaces[ surfIx ];
				if ( surface.pipelineObject[ DRAWPASS_POST_2D ] == INVALID_HANDLE ) {
					continue;
				}
				pipelineObject_t* pipelineObject;
				GetPipelineObject( surface.pipelineObject[ DRAWPASS_POST_2D ], &pipelineObject );
				vkCmdBindPipeline( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
				vkCmdBindDescriptorSets( graphicsQueue.commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, 1, &postPassState.descriptorSets[ i ], 0, nullptr );

				pushConstants_t pushConstants = { surface.objectId, surface.materialId };
				vkCmdPushConstants( graphicsQueue.commandBuffers[ i ], pipelineObject->pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof( pushConstants_t ), &pushConstants );

				vkCmdDrawIndexed( graphicsQueue.commandBuffers[ i ], surface.indicesCnt, 1, surface.firstIndex, surface.vertexOffset, 0 );
			}

#if defined( USE_IMGUI )
			ImGui_ImplVulkan_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin( "Control Panel" );
			ImGui::InputFloat( "Heightmap Height", &imguiControls.heightMapHeight, 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map R", &imguiControls.toneMapColor[ 0 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map G", &imguiControls.toneMapColor[ 1 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map B", &imguiControls.toneMapColor[ 2 ], 0.1f, 1.0f );
			ImGui::InputFloat( "Tone Map A", &imguiControls.toneMapColor[ 3 ], 0.1f, 1.0f );
			ImGui::InputInt( "Image Id", &imguiControls.dbgImageId );
			ImGui::Text( "Mouse: (%f, %f )", (float)window.input.GetMouse().x, (float)window.input.GetMouse().y );
			ImGui::Text( "Mouse Dt: (%f, %f )", (float)window.input.GetMouse().dx, (float)window.input.GetMouse().dy );
			const glm::vec2 screenPoint = glm::vec2( (float)window.input.GetMouse().x, (float)window.input.GetMouse().y );
			const glm::vec2 ndc = 2.0f * screenPoint * glm::vec2( 1.0f / width, 1.0f / height ) - 1.0f;
			char entityName[ 256 ];
			if ( imguiControls.selectedModelId >= 0 ) {
				sprintf_s( entityName, "%i: %s", imguiControls.selectedModelId, modelLib.FindName( scene.entities[ imguiControls.selectedModelId ].modelId ) );
			}
			else {
				memset( &entityName[ 0 ], 0, 256 );
			}
			static glm::vec3 tempOrigin;
			ImGui::Text( "NDC: (%f, %f )", (float)ndc.x, (float)ndc.y );
			ImGui::Text( "Camera Origin: (%f, %f, %f)", scene.camera.GetOrigin().x, scene.camera.GetOrigin().y, scene.camera.GetOrigin().z );
			if ( ImGui::BeginCombo( "Models", entityName ) )
			{
				for ( int entityIx = 0; entityIx < scene.entities.size(); ++entityIx ) {
					const modelSource_t* model = modelLib.Find( scene.entities[ entityIx ].modelId );
					sprintf_s( entityName, "%i: %s", entityIx, modelLib.FindName( scene.entities[ entityIx ].modelId ) );
					if ( ImGui::Selectable( entityName, ( imguiControls.selectedModelId == entityIx ) ) ) {
						imguiControls.selectedModelId = entityIx;
						scene.entities[ entityIx ].flags = WIREFRAME;
						tempOrigin.x = scene.entities[ entityIx ].matrix[ 3 ][ 0 ];
						tempOrigin.y = scene.entities[ entityIx ].matrix[ 3 ][ 1 ];
						tempOrigin.z = scene.entities[ entityIx ].matrix[ 3 ][ 2 ];
					}
					else {
						scene.entities[ entityIx ].flags = (renderFlags)( scene.entities[ entityIx ].flags & ~WIREFRAME );
					}
				}
				ImGui::EndCombo();
			}
			ImGui::InputFloat( "Selected Model X: ", &imguiControls.selectedModelOrigin.x, 0.1f, 1.0f );
			ImGui::InputFloat( "Selected Model Y: ", &imguiControls.selectedModelOrigin.y, 0.1f, 1.0f );
			ImGui::InputFloat( "Selected Model Z: ", &imguiControls.selectedModelOrigin.z, 0.1f, 1.0f );

			if ( imguiControls.selectedModelId >= 0 ) {
				entity_t& entity = scene.entities[ imguiControls.selectedModelId ];
				entity.matrix[ 3 ][ 0 ] = tempOrigin.x + imguiControls.selectedModelOrigin.x;
				entity.matrix[ 3 ][ 1 ] = tempOrigin.y + imguiControls.selectedModelOrigin.y;
				entity.matrix[ 3 ][ 2 ] = tempOrigin.z + imguiControls.selectedModelOrigin.z;
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

	void CreateImage( uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, GpuImage& image, AllocatorVkMemory& memory )
	{
		VkImageCreateInfo imageInfo{ };
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>( width );
		imageInfo.extent.height = static_cast<uint32_t>( height );
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional

		if ( vkCreateImage( context.device, &imageInfo, nullptr, &image.vk_image ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create image!" );
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements( context.device, image.vk_image, &memRequirements );

		VkMemoryAllocateInfo allocInfo{ };
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( memRequirements.memoryTypeBits, properties );

		allocVk_t alloc;
		if ( memory.Allocate( memRequirements.alignment, memRequirements.size, alloc ) ) {
			vkBindImageMemory( context.device, image.vk_image, memory.GetMemoryResource(), alloc.GetOffset() );
		} else {
			throw std::runtime_error( "Buffer could not be allocated!" );
		}
	}

	void GenerateMipmaps( VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels )
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
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for ( uint32_t i = 1; i < mipLevels; i++ )
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
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[ 0 ] = { 0, 0, 0 };
			blit.dstOffsets[ 1 ] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

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

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
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
		const uint64_t size = 128 * MB_1;
		CreateBuffer( size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, sharedMemory );
	}

	void GenerateGpuPrograms()
	{
		const uint32_t programCount = gpuPrograms.Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram* prog = gpuPrograms.Find( i );
			CreateShaders( *prog );
		}
	}

	void UploadTextures()
	{
		const uint32_t textureCount = textureLib.Count();
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			texture_t* texture = textureLib.Find( i );
			CreateImage( texture->width, texture->height, texture->mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image, localMemory );

			TransitionImageLayout( texture->image.vk_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mipLevels );

			const VkDeviceSize currentOffset = stagingBuffer.GetSize();
			stagingBuffer.CopyData( texture->bytes, texture->sizeBytes );

			CopyBufferToImage( commandBuffer, stagingBuffer.GetVkObject(), currentOffset, texture->image.vk_image, static_cast<uint32_t>( texture->width ), static_cast<uint32_t>( texture->height ) );
			texture->uploaded = true;
		}
		EndSingleTimeCommands( commandBuffer );

		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			texture_t* texture = textureLib.Find( i );
			GenerateMipmaps( texture->image.vk_image, VK_FORMAT_R8G8B8A8_SRGB, texture->width, texture->height, texture->mipLevels );
		}

		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			texture_t* texture = textureLib.Find( i );
			texture->image.vk_view = CreateImageView( texture->image.vk_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, texture->mipLevels );
		}

		// Default Images
		CreateImage( ShadowMapWidth, ShadowMapHeight, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rc.whiteImage, localMemory );
		rc.whiteImage.vk_view = CreateImageView( rc.whiteImage.vk_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

		CreateImage( ShadowMapWidth, ShadowMapHeight, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rc.blackImage, localMemory );
		rc.blackImage.vk_view = CreateImageView( rc.blackImage.vk_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1 );
	}

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
	}

	void UpdateView()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

		const glm::vec3 lightPos0 = glm::vec4( glm::cos( time ), 0.0f, -6.0f, 0.0f );
		const glm::vec3 lightDir0 = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );

		int width;
		int height;
		window.GetWindowSize( width, height );
		renderView.viewport.width = static_cast<float>( width );
		renderView.viewport.height = static_cast<float>( height );

		renderView.lights[ 0 ].lightPos = glm::vec4( lightPos0[ 0 ], lightPos0[ 1 ], lightPos0[ 2 ], 0.0f );
		renderView.lights[ 0 ].intensity = glm::vec4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderView.lights[ 0 ].lightDir = glm::vec4( lightDir0[ 0 ], lightDir0[ 1 ], lightDir0[ 2 ], 0.0f );
		renderView.lights[ 1 ].lightPos = glm::vec4( 0.0f, glm::cos( time ), -1.0f, 0.0 );
		renderView.lights[ 1 ].intensity = glm::vec4( 1.0f, 0.0f, 0.0f, 1.0f );
		renderView.lights[ 1 ].lightDir = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );
		renderView.lights[ 2 ].lightPos = glm::vec4( glm::sin( time ), 0.0f, -1.0f, 0.0f );
		renderView.lights[ 2 ].intensity = glm::vec4( 0.0f, 1.0f, 0.0f, 1.0f );
		renderView.lights[ 2 ].lightDir = glm::vec4( 0.0f, 0.0f, 1.0f, 0.0f );

		const glm::vec3 shadowLightPos = lightPos0;
		const glm::vec3 shadowLightDir = lightDir0;

		// Temp shadow map set-up
		shadowView.viewport = renderView.viewport;
		shadowView.viewMatrix = MatrixFromVector( shadowLightDir );
		shadowView.viewMatrix = glm::transpose( shadowView.viewMatrix );
		shadowView.viewMatrix[ 3 ][ 0 ] = shadowLightPos[ 0 ];
		shadowView.viewMatrix[ 3 ][ 1 ] = shadowLightPos[ 1 ];
		shadowView.viewMatrix[ 3 ][ 2 ] = shadowLightPos[ 2 ];

		Camera shadowCam;
		shadowCam = Camera( glm::vec4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		shadowCam.fov = glm::radians( 90.0f );
		shadowCam.far = nearPlane;
		shadowCam.near = farPlane;
		shadowCam.aspect = ( ShadowMapWidth / (float)ShadowMapHeight );
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

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkDestroyBuffer( context.device, frameState[ i ].globalConstants.GetVkObject(), nullptr );
			vkDestroyBuffer( context.device, frameState[ i ].surfParms.GetVkObject(), nullptr );
			vkDestroyBuffer( context.device, frameState[ i ].materialBuffers.GetVkObject(), nullptr );
			vkDestroyBuffer( context.device, frameState[ i ].lightParms.GetVkObject(), nullptr );
		}

		const uint32_t textureCount = textureLib.Count();
		for ( uint32_t i = 0; i < textureCount; ++i )
		{
			const texture_t* texture = textureLib.Find( i );
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

		vkDestroyCommandPool( context.device, graphicsQueue.commandPool, nullptr );
		vkDestroyDevice( context.device, nullptr );

		if ( enableValidationLayers )
		{
			DestroyDebugUtilsMessengerEXT( context.instance, debugMessenger, nullptr );
		}

		vkDestroySurfaceKHR( context.instance, window.vk_surface, nullptr );
		vkDestroyInstance( context.instance, nullptr );

		window.~Window();
	}

	void UpdatePostProcessBuffers( uint32_t currentImage )
	{
		std::vector< uniformBufferObject_t > uboBuffer;
		uboBuffer.reserve( MaxSurfaces );
		for ( uint32_t i = 0; i < MaxSurfaces; ++i )
		{
			uniformBufferObject_t ubo;
			ubo.model = glm::identity<glm::mat4>();
			ubo.view = glm::identity<glm::mat4>();
			ubo.proj = glm::identity<glm::mat4>();
			uboBuffer.push_back( ubo );
		}

		void* uboMemoryMap = frameState[ currentImage ].surfParms.alloc.GetPtr();
		if ( uboMemoryMap != nullptr ) {
			memcpy( uboMemoryMap, uboBuffer.data(), sizeof( uniformBufferObject_t ) * uboBuffer.size() );
		}
	}

	void UpdateBufferContents( uint32_t currentImage )
	{
		std::vector< globalUboConstants_t > globalsBuffer;
		globalsBuffer.reserve( 1 );
		{
			globalUboConstants_t globals;
			static auto startTime = std::chrono::high_resolution_clock::now();
			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

			float intPart = 0;
			const float fracPart = modf( time, &intPart );

			globals.time = glm::vec4( time, intPart, fracPart, 1.0f );
			globals.heightmap = glm::vec4( imguiControls.heightMapHeight, 0.0f, 0.0f, 0.0f );
			globals.tonemap = glm::vec4( imguiControls.toneMapColor[ 0 ], imguiControls.toneMapColor[ 1 ], imguiControls.toneMapColor[ 2 ], imguiControls.toneMapColor[ 3 ] );
			globalsBuffer.push_back( globals );
		}

		std::vector< uniformBufferObject_t > uboBuffer;
		uboBuffer.reserve( MaxSurfaces );
		assert( renderView.committedModelCnt < MaxModels );
		for ( uint32_t i = 0; i < renderView.committedModelCnt; ++i )
		{
			uniformBufferObject_t ubo;
			ubo.model = renderView.surfaces[ i ].modelMatrix;
			ubo.view = renderView.viewMatrix;
			ubo.proj = renderView.projMatrix;
			uboBuffer.push_back( ubo );
		}
		uboBuffer.resize( MaxModels );
		assert( shadowView.committedModelCnt < MaxModels );
		for ( uint32_t i = 0; i < shadowView.committedModelCnt; ++i )
		{
			uniformBufferObject_t ubo;
			ubo.model = shadowView.surfaces[ i ].modelMatrix;
			ubo.view = shadowView.viewMatrix;
			ubo.proj = shadowView.projMatrix;
			uboBuffer.push_back( ubo );
		}

		std::vector< materialBufferObject_t > materialBuffer;
		materialBuffer.reserve( materialLib.Count() );
		int materialAllocIx = 0;
		const uint32_t materialCount = materialLib.Count();
		for ( uint32_t i = 0; i < materialLib.Count(); ++i )
		{
			const Material* m = materialLib.Find( i );

			materialBufferObject_t ubo;
			for ( uint32_t t = 0; t < Material::MaxMaterialTextures; ++t ) {
				ubo.textures[ t ] = m->textures[ t ].Get();
			}
			materialBuffer.push_back( ubo );
		}

		std::vector< light_t > lightBuffer;
		lightBuffer.reserve( MaxLights );
		for ( int i = 0; i < MaxLights; ++i )
		{
			lightBuffer.push_back( renderView.lights[ i ] );
		}

		frameState[ currentImage ].globalConstants.Reset();
		frameState[ currentImage ].globalConstants.CopyData( globalsBuffer.data(), sizeof( globalUboConstants_t ) * globalsBuffer.size() );

		frameState[ currentImage ].surfParms.Reset();
		frameState[ currentImage ].surfParms.CopyData( uboBuffer.data(), sizeof( uniformBufferObject_t ) * uboBuffer.size() );

		frameState[ currentImage ].materialBuffers.Reset();
		frameState[ currentImage ].materialBuffers.CopyData( materialBuffer.data(), sizeof( materialBufferObject_t ) * materialBuffer.size() );

		frameState[ currentImage ].lightParms.Reset();
		frameState[ currentImage ].lightParms.CopyData( lightBuffer.data(), sizeof( light_t ) * lightBuffer.size() );
	}

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