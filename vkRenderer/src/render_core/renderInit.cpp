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
#include "../render_state/rhi.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/bindings.h"

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
		SetupMarkers();
		g_swapChain.Create( &gWindow, DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT );
	}

	{
		// Pool Creation
		CreateDescriptorPool();
		CreateCommandPools();
	}

	{
		particleShaderBinds = ShaderBindSet( g_particleCsBindings, g_particleCsBindCount );
		defaultBindSet = ShaderBindSet( g_defaultBindings, g_defaultBindCount );

		defaultBindSet.Create();
		particleShaderBinds.Create();

		for( uint32_t i = 0; i < MAX_FRAMES_STATES; ++i )
		{
			mainPassState.parms[i] = RegisterBindParm( &defaultBindSet );
			shadowPassState.parms[i] = RegisterBindParm( &defaultBindSet );
			postPassState.parms[i] = RegisterBindParm( &defaultBindSet );
			particleState.parms[i] = RegisterBindParm( &particleShaderBinds );
		}

		const uint32_t programCount = gAssets.gpuPrograms.Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram& prog = gAssets.gpuPrograms.Find( i )->Get();
			for ( uint32_t i = 0; i < prog.shaderCount; ++i )
			{
				if( prog.shaders[ i ].type == shaderType_t::COMPUTE ) {
					prog.bindset = &particleShaderBinds;
				} else {
					prog.bindset = &defaultBindSet;
				}
			}
		}

		AllocRegisteredBindParms();
	}

	InitShaderResources();

	CreateTextureSamplers();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	{
		// Create Frame Resources
		AllocateDeviceMemory( MaxFrameBufferMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameBufferMemory );

		CreateSyncObjects();
		CreateFramebuffers();
		CreatePipelineObjects();
		CreateCommandBuffers();
	}
}


void Renderer::InitShaderResources()
{
	{
		// Memory Allocations
		VkMemoryPropertyFlagBits type = VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		AllocateDeviceMemory( MaxSharedMemory, type, sharedMemory );
		type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		AllocateDeviceMemory( MaxLocalMemory, type, localMemory );
	}

	GenerateGpuPrograms( gAssets.gpuPrograms );

	CreateCodeTextures();
	CreateBuffers();
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
	ImGui_ImplVulkan_Init( &vkInfo, postPassState.fb[ 0 ]->GetVkRenderPass( false, true ) );
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
	gImguiControls.rasterizeScene = false;
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


	std::vector<const char*> enabledExtensions;
	for( auto ext : deviceExtensions )
	{
		enabledExtensions.push_back( ext );
	}
	std::vector<const char*> captureExtensions = { VK_EXT_DEBUG_MARKER_EXTENSION_NAME };
	if( IsDeviceSuitable( context.physicalDevice, gWindow.vk_surface, captureExtensions ) ) {
		for ( const char* ext : captureExtensions ) {
			enabledExtensions.push_back( ext );
		}
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>( enabledExtensions.size() );
	createInfo.ppEnabledExtensionNames = enabledExtensions.data();
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
		for ( uint32_t i = 0; i < prog.shaderCount; ++i ) {
			prog.vk_shaders[ i ] = vk_CreateShaderModule( prog.shaders[ i ].blob );
			prog.pipeline = INVALID_HDL;
		}
	}
}


void Renderer::CreatePipelineObjects()
{
	g_pipelineLib.clear();
	for ( uint32_t i = 0; i < gAssets.materialLib.Count(); ++i )
	{
		const Material& m = gAssets.materialLib.Find( i )->Get();

		for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
			Asset<GpuProgram>* prog = gAssets.gpuPrograms.Find( m.GetShader( passIx ) );
			if ( prog == nullptr ) {
				continue;
			}

			const drawPass_t drawPass = drawPass_t( passIx );

			pipelineState_t state;
			state.viewport = GetDrawPassViewport( drawPass );
			state.stateBits = GetStateBitsForDrawPass( drawPass );
			state.samplingRate = GetSampleCountForDrawPass( drawPass );
			state.shaders = &prog->Get();
			state.hash = prog->Handle().Get();

			VkRenderPass pass;
			VkDescriptorSetLayout layout;
			if ( passIx == DRAWPASS_SHADOW ) {
				pass = shadowPassState.fb[ 0 ]->GetVkRenderPass();
			}
			else if ( passIx == DRAWPASS_POST_2D ) {
				pass = postPassState.fb[ 0 ]->GetVkRenderPass();
			}
			else {
				pass = mainPassState.fb[ 0 ]->GetVkRenderPass();
			}
			assert( pass != VK_NULL_HANDLE );

			layout = prog->Get().bindset->GetVkObject();

			// FIXME: the drawpass essentially splits the shader asset
			//assert( prog->Get().pipeline == INVALID_HDL );
			CreateGraphicsPipeline( layout, pass, state, prog->Get().pipeline );
		}
	}
	for ( uint32_t i = 0; i < gAssets.gpuPrograms.Count(); ++i )
	{
		Asset<GpuProgram>* prog = gAssets.gpuPrograms.Find( i );
		if ( prog == nullptr ) {
			continue;
		}
		if( prog->Get().shaderCount != 1 ) {
			continue;
		}
		if ( prog->Get().shaders[0].type != shaderType_t::COMPUTE ) {
			continue;
		}

		pipelineState_t state;
		state.shaders = &prog->Get();
		state.hash = prog->Handle().Get();

		CreateComputePipeline( particleShaderBinds.GetVkObject(), state, prog->Get().pipeline );
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


void Renderer::CreateFramebuffers()
{
	int width = 0;
	int height = 0;
	gWindow.GetWindowFrameBufferSize( width, height );

	// Shadow images
	for ( size_t i = 0; i < MAX_FRAMES_STATES; ++i )
	{
		textureInfo_t info{};
		info.width = ShadowMapWidth;
		info.height = ShadowMapHeight;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = TEXTURE_SMP_1;
		info.fmt = TEXTURE_FMT_D_32;
		info.type = TEXTURE_TYPE_2D;
		info.aspect = TEXTURE_ASPECT_DEPTH_FLAG;
		info.tiling = TEXTURE_TILING_MORTON;

		frameState[ i ].shadowMapImage.info = info;
		frameState[ i ].shadowMapImage.gpuImage = new GpuImage();

		CreateGpuImage( info, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, *frameState[i].shadowMapImage.gpuImage, frameBufferMemory );
		frameState[i].shadowMapImage.gpuImage->VkImageView() = CreateImageView( frameState[i].shadowMapImage );
	}

	// Main images
	for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
	{
		textureInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = config.mainColorSubSamples;
		info.fmt = TEXTURE_FMT_RGBA_16;
		info.type = TEXTURE_TYPE_2D;
		info.aspect = TEXTURE_ASPECT_COLOR_FLAG;
		info.tiling = TEXTURE_TILING_MORTON;

		VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

		frameState[ i ].viewColorImage.bytes = nullptr;
		frameState[ i ].viewColorImage.info = info;
		frameState[ i ].viewColorImage.gpuImage = new GpuImage();

		CreateGpuImage( info, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, *frameState[ i ].viewColorImage.gpuImage, frameBufferMemory );
		frameState[ i ].viewColorImage.gpuImage->VkImageView() = CreateImageView( frameState[ i ].viewColorImage );

		VkFormat depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		info.fmt = TEXTURE_FMT_D_32_S8;
		info.type = TEXTURE_TYPE_2D;
		info.aspect = textureAspectFlags_t( TEXTURE_ASPECT_DEPTH_FLAG | TEXTURE_ASPECT_STENCIL_FLAG );

		frameState[ i ].depthImage.info = info;
		frameState[ i ].depthImage.bytes = nullptr;
		frameState[ i ].depthImage.gpuImage = new GpuImage();

		frameState[ i ].stencilImage.info = info;
		frameState[ i ].stencilImage.bytes = nullptr;
		frameState[ i ].stencilImage.gpuImage = new GpuImage();

		CreateGpuImage( info, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, *frameState[ i ].depthImage.gpuImage, frameBufferMemory );
		
		frameState[ i ].depthImage.info.aspect = TEXTURE_ASPECT_DEPTH_FLAG;
		frameState[ i ].depthImage.gpuImage->VkImageView() = CreateImageView( frameState[ i ].depthImage );

		frameState[ i ].depthImage.info.aspect = TEXTURE_ASPECT_STENCIL_FLAG;
		frameState[ i ].stencilImage.gpuImage->VkImageView() = CreateImageView( frameState[ i ].depthImage );
	}

	// Shadow map
	for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
	{
		frameBufferCreateInfo_t fbInfo = {};
		//fbInfo.color0 = &rc.whiteImage;
		fbInfo.depth = &frameState[ i ].shadowMapImage;
		fbInfo.width = frameState[ i ].shadowMapImage.info.width;
		fbInfo.height = frameState[ i ].shadowMapImage.info.height;

		shadowMap[ i ].Create( fbInfo );

		shadowPassState.x = 0;
		shadowPassState.y = 0;
		shadowPassState.width = shadowMap[ i ].width;
		shadowPassState.height = shadowMap[ i ].height;
		shadowPassState.fb[i] = &shadowMap[i];
		shadowPassState.readAfter = true;
		shadowPassState.presentAfter = false;
	}

	// Main Scene 3D Render
	for ( size_t i = 0; i < g_swapChain.GetBufferCount(); i++ )
	{
		frameBufferCreateInfo_t fbInfo = {};
		fbInfo.color0 = &frameState[ i ].viewColorImage;
		fbInfo.depth = &frameState[ i ].depthImage;
		fbInfo.stencil = &frameState[ i ].stencilImage;
		fbInfo.width = frameState[ i ].viewColorImage.info.width;
		fbInfo.height = frameState[ i ].viewColorImage.info.height;

		mainColor[ i ].Create( fbInfo );

		mainPassState.x = 0;
		mainPassState.y = 0;
		mainPassState.width = width;
		mainPassState.height = height;
		mainPassState.fb[i] = &mainColor[i];
		mainPassState.readAfter = true;
		mainPassState.presentAfter = false;
	}

	// Swap Chain Images
	const uint32_t swapChainCount = g_swapChain.GetBufferCount();
	for ( size_t i = 0; i < swapChainCount; i++ )
	{
		postPassState.x = 0;
		postPassState.y = 0;
		postPassState.width = g_swapChain.GetWidth();
		postPassState.height = g_swapChain.GetHeight();
		postPassState.fb[ i ] = &g_swapChain.framebuffers[ i ];
		postPassState.readAfter = false;
		postPassState.presentAfter = true;
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


ShaderBindParms* Renderer::RegisterBindParm( const ShaderBindSet* set )
{
	ShaderBindParms parms = ShaderBindParms( set );

	bindParmsList[ bindParmCount ] = parms;
	++bindParmCount;

	return &bindParmsList[ bindParmCount - 1 ];
}


void Renderer::AllocRegisteredBindParms()
{
	std::vector<VkDescriptorSetLayout> layouts;
	std::vector<VkDescriptorSet> descSets;

	layouts.reserve( bindParmCount );
	descSets.reserve( bindParmCount );

	for ( uint32_t i = 0; i < bindParmCount; ++i )
	{
		ShaderBindParms& parms = bindParmsList[i];
		const ShaderBindSet* set = parms.GetSet();

		layouts.push_back( set->GetVkObject() );
		descSets.push_back( VK_NULL_HANDLE );
	}

	VkDescriptorSetAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>( layouts.size() );
	allocInfo.pSetLayouts = layouts.data();

	if ( vkAllocateDescriptorSets( context.device, &allocInfo, descSets.data() ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to allocate descriptor sets!" );
	}

	for ( uint32_t i = 0; i < bindParmCount; ++i )
	{
		ShaderBindParms& parms = bindParmsList[ i ];
		parms.SetVkObject( descSets[i] );
	}
}


void Renderer::CreateBuffers()
{
	for ( size_t i = 0; i < g_swapChain.GetBufferCount(); ++i )
	{
		frameState[ i ].globalConstants.Create( "Globals", 1, sizeof( viewBufferObject_t ), bufferType_t::UNIFORM, sharedMemory );
		frameState[ i ].viewParms.Create( "View", MaxViews, sizeof( viewBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState[ i ].surfParms.Create( "Surf", MaxViews * MaxSurfaces, sizeof( uniformBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState[ i ].materialBuffers.Create( "Material", MaxMaterials, sizeof( materialBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState[ i ].lightParms.Create( "Light", MaxLights, sizeof( lightBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState[ i ].particleBuffer.Create( "Particle", MaxParticles, sizeof( particleBufferObject_t ), bufferType_t::STORAGE, sharedMemory );

		for ( size_t v = 0; v < g_swapChain.GetBufferCount(); ++v ) {
			frameState[ i ].surfParmPartitions[ v ] = frameState[ i ].surfParms.GetView( v * MaxSurfaces, MaxSurfaces );
		}
	}

	vb.Create( "VB", MaxVertices, sizeof( vsInput_t ), bufferType_t::VERTEX, localMemory );
	ib.Create( "IB", MaxIndices, sizeof( uint32_t ), bufferType_t::INDEX, localMemory );

	stagingBuffer.Create( "Staging", 1, 256 * MB_1, bufferType_t::STAGING, sharedMemory );
}


void Renderer::CreateGpuImage( const textureInfo_t& info, VkImageUsageFlags usage, GpuImage& image, AllocatorVkMemory& memory )
{
	VkImageCreateInfo imageInfo{ };
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>( info.width );
	imageInfo.extent.height = static_cast<uint32_t>( info.height );
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = info.mipLevels;
	imageInfo.arrayLayers = info.layers;
	imageInfo.format = vk_GetTextureFormat( info.fmt );
	imageInfo.tiling = ( info.tiling == TEXTURE_TILING_LINEAR ) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = vk_GetSampleCount( info.subsamples );

	VkImageStencilUsageCreateInfo stencilUsage{};
	if ( ( info.aspect & ( TEXTURE_ASPECT_DEPTH_FLAG | TEXTURE_ASPECT_STENCIL_FLAG ) ) != 0 )
	{
		stencilUsage.sType = VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO;
		stencilUsage.stencilUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		imageInfo.pNext = &stencilUsage;
	}
	else {
		imageInfo.flags = ( info.type == TEXTURE_TYPE_CUBE ) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	}

	if ( vkCreateImage( context.device, &imageInfo, nullptr, &image.VkImage() ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create image!" );
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements( context.device, image.GetVkImage(), &memRequirements );

	VkMemoryAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = memory.memoryTypeIndex;

	alloc_t<Allocator<VkDeviceMemory>> alloc;
	if ( memory.Allocate( memRequirements.alignment, memRequirements.size, alloc ) ) {
		vkBindImageMemory( context.device, image.GetVkImage(), memory.GetMemoryResource(), alloc.GetOffset() );
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
	info.subsamples = TEXTURE_SMP_1;
	info.fmt = TEXTURE_FMT_BGRA_8;
	info.aspect = TEXTURE_ASPECT_COLOR_FLAG;
	info.type = TEXTURE_TYPE_2D;
	info.tiling = TEXTURE_TILING_MORTON;

	rc.whiteImage.info = info;
	rc.whiteImage.bytes = nullptr;
	rc.whiteImage.gpuImage = new GpuImage();

	rc.blackImage.info = info;
	rc.blackImage.bytes = nullptr;
	rc.blackImage.gpuImage = new GpuImage();

	// Default Images
	CreateGpuImage( info, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, *rc.whiteImage.gpuImage, localMemory );
	rc.whiteImage.gpuImage->VkImageView() = CreateImageView( rc.whiteImage );

	CreateGpuImage( info, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, *rc.blackImage.gpuImage, localMemory );
	rc.blackImage.gpuImage->VkImageView() = CreateImageView( rc.blackImage );
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

	if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, computeQueue.semaphores ) ) {
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
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, computeQueue.commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate compute command buffers!" );
		}
		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( computeQueue.commandBuffers[i], 0 );
		}
	}
}