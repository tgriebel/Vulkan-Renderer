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
#include "../render_state/rhi.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/bindings.h"

#include "drawpass.h"
#include "swapChain.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../external/imgui/backends/imgui_impl_vulkan.h"
#endif


void Renderer::Init()
{
	InitApi();

	renderView.region = renderViewRegion_t::STANDARD_RASTER;
	renderView.name = "Main View";

	shadowView.region = renderViewRegion_t::SHADOW;
	shadowView.name = "Shadow View";

	view2D.region = renderViewRegion_t::POST;
	view2D.name = "Post View";

	InitRenderPasses( shadowView, shadowMap );
	InitRenderPasses( renderView, mainColor );
	InitRenderPasses( view2D, g_swapChain.framebuffers );

	InitShaderResources();

	CreatePipelineObjects();

	InitImGui( view2D );
}


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


void Renderer::InitApi()
{
	{
		// Device Set-up
		CreateInstance();
		SetupDebugMessenger();
		g_window.CreateSurface();
		CreateDevice();
		SetupMarkers();
		g_swapChain.Create( &g_window, DEFAULT_DISPLAY_WIDTH, DEFAULT_DISPLAY_HEIGHT );
	}

	{
		// Pool Creation
		CreateDescriptorPool();
		CreateCommandPools();
	}

	{
		// Memory Allocations
		VkMemoryPropertyFlagBits type = VkMemoryPropertyFlagBits( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		vk_AllocateDeviceMemory( MaxSharedMemory, type, sharedMemory );
		type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		vk_AllocateDeviceMemory( MaxLocalMemory, type, localMemory );
	}

	InitConfig();
	CreateTextureSamplers();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	{
		// Create Frame Resources
		vk_AllocateDeviceMemory( MaxFrameBufferMemory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, frameBufferMemory );

		CreateSyncObjects();
		CreateFramebuffers();
		CreateCommandBuffers();
	}
}


void Renderer::InitShaderResources()
{
	{
		particleShaderBinds = ShaderBindSet( g_particleCsBindings, g_particleCsBindCount );
		defaultBindSet = ShaderBindSet( g_defaultBindings, g_defaultBindCount );

		defaultBindSet.Create();
		particleShaderBinds.Create();

		const uint32_t programCount = g_assets.gpuPrograms.Count();
		for ( uint32_t i = 0; i < programCount; ++i )
		{
			GpuProgram& prog = g_assets.gpuPrograms.Find( i )->Get();
			for ( uint32_t i = 0; i < prog.shaderCount; ++i )
			{
				if ( prog.shaders[ i ].type == shaderType_t::COMPUTE ) {
					prog.bindset = &particleShaderBinds;
				}
				else {
					prog.bindset = &defaultBindSet;
				}
			}
		}
	}

	const uint32_t frameStateCount = g_swapChain.GetBufferCount();
	RenderView* views[ 3 ] = { &shadowView, &renderView, &view2D }; // FIXME: TEMP!!
	for ( uint32_t viewIx = 0; viewIx < 3; ++viewIx )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = views[ viewIx ]->passes[ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			for ( uint32_t i = 0; i < frameStateCount; ++i ) {
				pass->parms[ i ] = RegisterBindParm( &defaultBindSet );
			}
		}
	}

	for ( uint32_t i = 0; i < MAX_FRAMES_STATES; ++i ) {
		particleState.parms[ i ] = RegisterBindParm( &particleShaderBinds );
	}

	AllocRegisteredBindParms();

	GenerateGpuPrograms( g_assets.gpuPrograms );

	CreateCodeTextures();
	CreateBuffers();
}


void Renderer::InitImGui( RenderView& view )
{
#if defined( USE_IMGUI )
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan( g_window.window, true );

	ImGui_ImplVulkan_InitInfo vkInfo = {};
	vkInfo.Instance = context.instance;
	vkInfo.PhysicalDevice = context.physicalDevice;
	vkInfo.Device = context.device;
	vkInfo.QueueFamily = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
	vkInfo.Queue = context.gfxContext;
	vkInfo.PipelineCache = nullptr;
	vkInfo.DescriptorPool = descriptorPool;
	vkInfo.Allocator = nullptr;
	vkInfo.MinImageCount = 2;
	vkInfo.ImageCount = 2;
	vkInfo.CheckVkResultFn = nullptr;

	assert( view.passes[ DRAWPASS_POST_2D ] != nullptr );
	const renderPassTransitionFlags_t transitionState = view.passes[ DRAWPASS_POST_2D ]->transitionState;
	ImGui_ImplVulkan_Init( &vkInfo, view.passes[ DRAWPASS_POST_2D ]->fb[ 0 ]->GetVkRenderPass( transitionState ) );

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	vkResetCommandPool( context.device, gfxContext.commandPool, 0 );

	// Upload Fonts
	{
		BeginUploadCommands( uploadContext );
		VkCommandBuffer commandBuffer = uploadContext.commandBuffer;
		ImGui_ImplVulkan_CreateFontsTexture( commandBuffer );
		EndUploadCommands( uploadContext );

		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}
	ImGui_ImplGlfw_NewFrame();
#endif
	g_imguiControls.raytraceScene = false;
	g_imguiControls.rasterizeScene = false;
	g_imguiControls.rebuildRaytraceScene = false;
	g_imguiControls.rebuildShaders = false;
	g_imguiControls.heightMapHeight = 1.0f;
	g_imguiControls.roughness = 0.9f;
	g_imguiControls.shadowStrength = 0.99f;
	g_imguiControls.toneMapColor[ 0 ] = 1.0f;
	g_imguiControls.toneMapColor[ 1 ] = 1.0f;
	g_imguiControls.toneMapColor[ 2 ] = 1.0f;
	g_imguiControls.toneMapColor[ 3 ] = 1.0f;
	g_imguiControls.dbgImageId = -1;
	g_imguiControls.selectedEntityId = -1;
	g_imguiControls.selectedModelOrigin = vec3f( 0.0f );
}


void Renderer::CreateDevice()
{
	// Pick physical device
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
			if ( vk_IsDeviceSuitable( device, g_window.vk_surface, deviceExtensions ) )
			{
				vkGetPhysicalDeviceProperties( device, &context.deviceProperties );
				context.physicalDevice = device;
				break;
			}
		}

		if ( context.physicalDevice == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to find a suitable GPU!" );
		}
	}

	// Create logical device
	{
		QueueFamilyIndices indices = vk_FindQueueFamilies( context.physicalDevice, g_window.vk_surface );
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
		if( vk_IsDeviceSuitable( context.physicalDevice, g_window.vk_surface, captureExtensions ) ) {
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

		vkGetDeviceQueue( context.device, indices.graphicsFamily.value(), 0, &context.gfxContext );
		vkGetDeviceQueue( context.device, indices.presentFamily.value(), 0, &context.presentQueue );
		vkGetDeviceQueue( context.device, indices.computeFamily.value(), 0, &context.computeContext );
	}
}


void Renderer::GenerateGpuPrograms( AssetLibGpuProgram& lib )
{
	const uint32_t programCount = lib.Count();
	for ( uint32_t i = 0; i < programCount; ++i )
	{
		GpuProgram& prog = lib.Find( i )->Get();
		for ( uint32_t i = 0; i < prog.shaderCount; ++i ) {
			prog.vk_shaders[ i ] = vk_CreateShaderModule( prog.shaders[ i ].blob );
		}
	}
}


void Renderer::CreatePipelineObjects()
{
	ClearPipelineCache();
	for ( uint32_t i = 0; i < g_assets.materialLib.Count(); ++i )
	{
		const Material& m = g_assets.materialLib.Find( i )->Get();

		for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
			const hdl_t progHdl = m.GetShader( passIx );
			Asset<GpuProgram>* prog = g_assets.gpuPrograms.Find( progHdl );
			if ( prog == nullptr ) {
				continue;
			}

			const drawPass_t drawPass = drawPass_t( passIx );

			const DrawPass* pass = GetDrawPass( drawPass );
			assert( pass != nullptr );

			CreateGraphicsPipeline( pass, *prog );
		}
	}
	for ( uint32_t i = 0; i < g_assets.gpuPrograms.Count(); ++i )
	{
		Asset<GpuProgram>* prog = g_assets.gpuPrograms.Find( i );
		if ( prog == nullptr ) {
			continue;
		}
		if( prog->Get().shaderCount != 1 ) {
			continue;
		}
		if ( prog->Get().shaders[0].type != shaderType_t::COMPUTE ) {
			continue;
		}

		CreateComputePipeline( *prog );
	}
}


void Renderer::InitRenderPasses( RenderView& view, FrameBuffer fb[ MAX_FRAMES_STATES ] )
{
	const uint32_t frameStateCount = g_swapChain.GetBufferCount();
	const uint32_t width = g_swapChain.GetWidth();
	const uint32_t height = g_swapChain.GetHeight();

	for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
	{
		view.passes[ passIx ] = nullptr;

		if( passIx < ViewRegionPassBegin( view.region ) ) {
			continue;
		}
		if ( passIx > ViewRegionPassEnd( view.region ) ) {
			continue;
		}
		DrawPass* pass = new DrawPass();
		
		pass->name = GetPassDebugName( drawPass_t( passIx ) );
		pass->viewport.x = 0;
		pass->viewport.y = 0;
		pass->viewport.width = fb[ 0 ].width;
		pass->viewport.height = fb[ 0 ].height;
		for ( uint32_t i = 0; i < frameStateCount; ++i ) {
			pass->fb[ i ] = &fb[ i ];
		}

		pass->transitionState.bits = 0;
		pass->stateBits = GFX_STATE_NONE;

		if( passIx == DRAWPASS_SHADOW )
		{
			pass->transitionState.flags.clear = true;
			pass->transitionState.flags.store = true;
			pass->transitionState.flags.readAfter = true;
			pass->transitionState.flags.presentAfter = false;

			pass->stateBits |= GFX_STATE_DEPTH_TEST;
			pass->stateBits |= GFX_STATE_DEPTH_WRITE;
			pass->stateBits |= GFX_STATE_DEPTH_OP_0;

			pass->clearColor = vec4f( 0.0f, 0.0f, 0.0f, 1.0f );
			pass->clearDepth = 1.0f;
			pass->clearStencil = 0;

			pass->sampleRate = imageSamples_t::IMAGE_SMP_1;
		}
		else if( passIx == DRAWPASS_POST_2D )
		{
			pass->transitionState.flags.clear = true;
			pass->transitionState.flags.store = true;
			pass->transitionState.flags.readAfter = false;
			pass->transitionState.flags.presentAfter = true;

			pass->clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
			pass->clearDepth = 0.0f;
			pass->clearStencil = 0;

			pass->stateBits |= GFX_STATE_BLEND_ENABLE;
			pass->sampleRate = imageSamples_t::IMAGE_SMP_1;
		}
		else
		{
			pass->transitionState.flags.clear = false;
			pass->transitionState.flags.store = true;
			pass->transitionState.flags.readAfter = true;
			pass->transitionState.flags.presentAfter = false;

			gfxStateBits_t stateBits = GFX_STATE_NONE;
			if( passIx == DRAWPASS_DEPTH )
			{
				stateBits |= GFX_STATE_DEPTH_TEST;
				stateBits |= GFX_STATE_DEPTH_WRITE;
				stateBits |= GFX_STATE_COLOR_MASK;
				stateBits |= GFX_STATE_MSAA_ENABLE;
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_STENCIL_ENABLE;

				pass->transitionState.flags.clear = true;
			}
			else if ( passIx == DRAWPASS_TRANS )
			{
				stateBits |= GFX_STATE_DEPTH_TEST;
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_MSAA_ENABLE;
				stateBits |= GFX_STATE_BLEND_ENABLE;
			}
			else if ( passIx == DRAWPASS_DEBUG_WIREFRAME )
			{
				stateBits |= GFX_STATE_WIREFRAME_ENABLE;
				stateBits |= GFX_STATE_MSAA_ENABLE;

				pass->transitionState.flags.readAfter = true;
			}
			else if ( passIx == DRAWPASS_DEBUG_SOLID )
			{
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_BLEND_ENABLE;
				stateBits |= GFX_STATE_MSAA_ENABLE;
			}
			else
			{
				stateBits |= GFX_STATE_DEPTH_TEST;
				stateBits |= GFX_STATE_DEPTH_WRITE;
				stateBits |= GFX_STATE_CULL_MODE_BACK;
				stateBits |= GFX_STATE_MSAA_ENABLE;
			}
			pass->stateBits = stateBits;

			pass->clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
			pass->clearDepth = 0.0f;
			pass->clearStencil = 0;

			pass->sampleRate = config.mainColorSubSamples;
		}

		view.passes[ passIx ] = pass;
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
	g_window.GetWindowFrameBufferSize( width, height );

	// Shadow images
	for ( size_t i = 0; i < MAX_FRAMES_STATES; ++i )
	{
		imageInfo_t info{};
		info.width = ShadowMapWidth;
		info.height = ShadowMapHeight;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = IMAGE_SMP_1;
		info.fmt = IMAGE_FMT_D_32;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		CreateImage( info, GPU_IMAGE_READ, frameBufferMemory, frameState[ i ].shadowMapImage );
	}

	// Main images
	for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ )
	{
		imageInfo_t info{};
		info.width = width;
		info.height = height;
		info.mipLevels = 1;
		info.layers = 1;
		info.subsamples = config.mainColorSubSamples;
		info.fmt = IMAGE_FMT_RGBA_16;
		info.type = IMAGE_TYPE_2D;
		info.aspect = IMAGE_ASPECT_COLOR_FLAG;
		info.tiling = IMAGE_TILING_MORTON;

		CreateImage( info, GPU_IMAGE_READ, frameBufferMemory, frameState[ i ].viewColorImage );

		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;
		info.aspect = imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG );

		CreateImage( info, GPU_IMAGE_READ, frameBufferMemory, frameState[ i ].depthStencilImage );
		
		imageInfo_t depthInfo = frameState[ i ].depthStencilImage.info;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		frameState[ i ].depthImageView.Init( frameState[ i ].depthStencilImage, depthInfo );

		imageInfo_t stencilInfo = frameState[ i ].depthStencilImage.info;
		stencilInfo.aspect = IMAGE_ASPECT_STENCIL_FLAG;
		frameState[ i ].stencilImageView.Init( frameState[ i ].depthStencilImage, stencilInfo );
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
	}

	// Main Scene 3D Render
	for ( size_t i = 0; i < g_swapChain.GetBufferCount(); i++ )
	{
		frameBufferCreateInfo_t fbInfo = {};
		fbInfo.color0 = &frameState[ i ].viewColorImage;
		fbInfo.depth = &frameState[ i ].depthImageView;
		fbInfo.stencil = &frameState[ i ].stencilImageView;
		fbInfo.width = frameState[ i ].viewColorImage.info.width;
		fbInfo.height = frameState[ i ].viewColorImage.info.height;

		mainColor[ i ].Create( fbInfo );
	}
}


void Renderer::CreateCommandPools()
{
	VkCommandPoolCreateInfo poolInfo{ };
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &gfxContext.commandPool ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create graphics command pool!" );
	}

	poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_COMPUTE ];
	if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &computeContext.commandPool ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create compute command pool!" );
	}

	poolInfo.queueFamilyIndex = context.queueFamilyIndices[ QUEUE_GRAPHICS ];
	if ( vkCreateCommandPool( context.device, &poolInfo, nullptr, &uploadContext.commandPool ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create upload command pool!" );
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
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if ( vkCreateDescriptorPool( context.device, &poolInfo, nullptr, &descriptorPool ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to create descriptor pool!" );
	}
}


ShaderBindParms* Renderer::RegisterBindParm( const ShaderBindSet* set )
{
	ShaderBindParms parms = ShaderBindParms( set );

	assert( bindParmCount < DescriptorPoolMaxSets );
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


void Renderer::FreeRegisteredBindParms()
{
	std::vector<VkDescriptorSet> descSets;
	descSets.reserve( bindParmCount );

	for ( uint32_t i = 0; i < bindParmCount; ++i )
	{
		ShaderBindParms& parms = bindParmsList[ i ];
		descSets.push_back( parms.GetVkObject() );
	}

	vkFreeDescriptorSets( context.device, descriptorPool, static_cast<uint32_t>( descSets.size() ), descSets.data() );
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


void Renderer::CreateImage( const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage )
{
	outImage.bytes = nullptr;
	outImage.sizeBytes = 0;
	outImage.dirty = false;
	outImage.uploadId = -1;
	outImage.info = info;
	outImage.gpuImage = new GpuImage();
	outImage.gpuImage->Create( outImage.info, flags, memory );
}


void Renderer::CreateCodeTextures() {
	imageInfo_t info{};
	info.width = ShadowMapWidth;
	info.height = ShadowMapHeight;
	info.mipLevels = 1;
	info.layers = 1;
	info.subsamples = IMAGE_SMP_1;
	info.fmt = IMAGE_FMT_BGRA_8;
	info.aspect = IMAGE_ASPECT_COLOR_FLAG;
	info.type = IMAGE_TYPE_2D;
	info.tiling = IMAGE_TILING_MORTON;

	// Default Images
	CreateImage( info, GPU_IMAGE_READ, localMemory, rc.whiteImage );
	CreateImage( info, GPU_IMAGE_READ, localMemory, rc.blackImage );
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
		if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &gfxContext.imageAvailableSemaphores[ i ] ) != VK_SUCCESS ||
			vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, &gfxContext.renderFinishedSemaphores[ i ] ) != VK_SUCCESS ||
			vkCreateFence( context.device, &fenceInfo, nullptr, &gfxContext.inFlightFences[ i ] ) != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create synchronization objects for a frame!" );
		}
	}

	if ( vkCreateSemaphore( context.device, &semaphoreInfo, nullptr, computeContext.semaphores ) ) {
		throw std::runtime_error( "Failed to create compute semaphore!" );
	}
}


void Renderer::CreateCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo{ };
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	// Graphics
	{
		allocInfo.commandPool = gfxContext.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, gfxContext.commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate graphics command buffers!" );
		}

		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( gfxContext.commandBuffers[ i ], 0 );
		}
	}

	// Compute
	{
		allocInfo.commandPool = computeContext.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>( MAX_FRAMES_STATES );

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, computeContext.commandBuffers ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate compute command buffers!" );
		}
		for ( size_t i = 0; i < MAX_FRAMES_STATES; i++ ) {
			vkResetCommandBuffer( computeContext.commandBuffers[i], 0 );
		}
	}

	// Upload
	{
		allocInfo.commandPool = uploadContext.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if ( vkAllocateCommandBuffers( context.device, &allocInfo, &uploadContext.commandBuffer ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to allocate upload command buffers!" );
		}
		vkResetCommandBuffer( uploadContext.commandBuffer, 0 );
	}
}