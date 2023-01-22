#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"
#include <scene/entity.h>

void Renderer::CreateInstance()
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


void Renderer::InitVulkan()
{
	{
		// Device Set-up
		CreateInstance();
		SetupDebugMessenger();
		gWindow.CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		swapChain.Create( &gWindow, DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT );
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

	GenerateGpuPrograms( gAssets.gpuPrograms );

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


void Renderer::InitImGui()
{
#if defined( USE_IMGUI )
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan( gWindow.window, true );

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
	gImguiControls.raytraceScene = false;
	gImguiControls.rebuildRaytraceScene = false;
	gImguiControls.rebuildShaders = false;
	gImguiControls.heightMapHeight = 1.0f;
	gImguiControls.roughness = 0.9f;
	gImguiControls.shadowStrength = 0.99f;
	gImguiControls.toneMapColor[ 0 ] = 1.0f;
	gImguiControls.toneMapColor[ 1 ] = 1.0f;
	gImguiControls.toneMapColor[ 2 ] = 1.0f;
	gImguiControls.toneMapColor[ 3 ] = 1.0f;
	gImguiControls.dbgImageId = -1;
	gImguiControls.selectedEntityId = -1;
	gImguiControls.selectedModelOrigin = vec3f( 0.0f );
}


void Renderer::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies( context.physicalDevice, gWindow.vk_surface );
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
	deviceFeatures.sampleRateShading = VK_TRUE;
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


void Renderer::GenerateGpuPrograms( AssetLibGpuProgram& lib )
{
	const uint32_t programCount = lib.Count();
	for ( uint32_t i = 0; i < programCount; ++i )
	{
		GpuProgram& prog = lib.Find( i )->Get();
		for ( int i = 0; i < GpuProgram::MaxShaders; ++i ) {
			prog.vk_shaders[ i ] = CreateShaderModule( prog.shaders[ i ].blob );
		}
	}
}


void Renderer::CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, GpuBuffer& buffer, AllocatorVkMemory& bufferMemory )
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
	}
	else {
		throw std::runtime_error( "Buffer could not allocate!" );
	}
}


void Renderer::CreateResourceBuffers()
{
	stagingBuffer.Reset();
	const uint64_t size = 256 * MB_1;
	CreateBuffer( size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, sharedMemory );
}


void Renderer::CreatePipelineObjects()
{
	pipelineLib.Clear();
	for ( uint32_t i = 0; i < gAssets.materialLib.Count(); ++i )
	{
		const Material& m = gAssets.materialLib.Find( i )->Get();

		for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
			Asset<GpuProgram>* prog = gAssets.gpuPrograms.Find( m.GetShader( passIx ) );
			if ( prog == nullptr ) {
				continue;
			}

			pipelineState_t state;
			state.viewport = GetDrawPassViewport( (drawPass_t)passIx );
			state.stateBits = GetStateBitsForDrawPass( (drawPass_t)passIx );
			state.shaders = &prog->Get();
			state.tag = gAssets.gpuPrograms.FindName( m.GetShader( passIx ) );

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

			CreateGraphicsPipeline( layout, pass, state, prog->Get().pipeline );
		}
	}
}


void Renderer::CreateTextureSamplers()
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


void Renderer::CreateRenderPasses()
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


void Renderer::CreateFramebuffers()
{
	int width = 0;
	int height = 0;
	gWindow.GetWindowFrameBufferSize( width, height );

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


void Renderer::CreateCommandPools()
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


void Renderer::CreateDescriptorPool()
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


void Renderer::CreateDescriptorSets( VkDescriptorSetLayout& layout, VkDescriptorSet descSets[ MAX_FRAMES_STATES ] )
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


void Renderer::CreateDescSetLayouts()
{
	CreateSceneRenderDescriptorSetLayout( globalLayout );
	CreateSceneRenderDescriptorSetLayout( postProcessLayout );
	CreateDescriptorSets( globalLayout, mainPassState.descriptorSets );
	CreateDescriptorSets( globalLayout, shadowPassState.descriptorSets );
	CreateDescriptorSets( postProcessLayout, postPassState.descriptorSets );
}


void Renderer::CreateUniformBuffers()
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
			const VkDeviceSize stride = std::max( context.limits.minUniformBufferOffsetAlignment, sizeof( lightBufferObject_t ) );
			const VkDeviceSize lightBufferSize = MaxLights * stride;
			CreateBuffer( lightBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, frameState[ i ].lightParms, sharedMemory );
		}
	}
}


void Renderer::CreateImage( const textureInfo_t& info, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits numSamples, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, GpuImage& image, AllocatorVkMemory& memory )
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
	}
	else {
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
	}
	else {
		throw std::runtime_error( "Buffer could not be allocated!" );
	}
}


void Renderer::CreateCodeTextures() {
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


void Renderer::CreateSyncObjects()
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


void Renderer::CreateCommandBuffers()
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