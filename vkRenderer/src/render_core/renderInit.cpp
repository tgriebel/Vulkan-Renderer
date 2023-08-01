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

#include "debugMenu.h"

void Renderer::Init()
{
	InitApi();

	viewCount = 0;

	for ( uint32_t i = 0; i < MaxShadowViews; ++i )
	{
		shadowViews[ i ] = &views[ viewCount ];
		shadowViews[ i ]->Init( "Shadow View", renderViewRegion_t::SHADOW, viewCount, shadowMap[ i ] );
		++viewCount;
	}

	for ( uint32_t i = 0; i < Max3DViews; ++i )
	{
		renderViews[ i ] = &views[ viewCount ];
		renderViews[ i ]->Init( "Main View", renderViewRegion_t::STANDARD_RASTER, viewCount, mainColor );
		++viewCount;
	}

	{
		view2Ds[ 0 ] = &views[ viewCount ];
		view2Ds[ 0 ]->Init( "Present", renderViewRegion_t::POST, viewCount, g_swapChain.framebuffers );
		++viewCount;
	}

	assert( viewCount <= ( MaxViews - 1 ) ); // Last view is reserved for temp views

	for ( uint32_t i = 0; i < MaxShadowViews; ++i ) {
		shadowViews[ i ]->Commit();
	}
	renderViews[ 0 ]->Commit();
	view2Ds[ 0 ]->Commit();

	InitShaderResources();

	InitImGui( *view2Ds[ 0 ] );

	UploadAssets();
}


void Renderer::CreateInstance()
{
	if ( enableValidationLayers && !CheckValidationLayerSupport() )
	{
		throw std::runtime_error( "validation layers requested, but not available!" );
	}

	VkApplicationInfo appInfo{ };
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Extensia";
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

	if ( vkCreateInstance( &createInfo, nullptr, &context.instance ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create instance!" );
	}

	// Debug Messenger
	{
		if ( !enableValidationLayers ) {
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo( createInfo );

		if ( vk_CreateDebugUtilsMessengerEXT( context.instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to set up debug messenger!" );
		}
	}
}


void Renderer::InitApi()
{
	{
		// Device Set-up
		CreateInstance();
		g_window.CreateSurface();
		CreateDevice();

		int width, height;
		g_window.GetWindowSize( width, height );
		g_swapChain.Create( &g_window, width, height );
	}

	{
		// Pool Creation
		CreateDescriptorPool();
	}

	{
		// Memory Allocations
		sharedMemory.Create( MaxSharedMemory, memoryRegion_t::SHARED );
		localMemory.Create( MaxLocalMemory, memoryRegion_t::LOCAL );
	}

	InitConfig();

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	{
		// Create Frame Resources
		frameBufferMemory.Create( MaxFrameBufferMemory, memoryRegion_t::LOCAL );

		CreateSyncObjects();
		CreateFramebuffers();

		gfxContext.Create();
		computeContext.Create();
		uploadContext.Create();
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
	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = views[ viewIx ].passes[ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			for ( uint32_t i = 0; i < frameStateCount; ++i ) {
				pass->parms[ i ] = RegisterBindParm( &defaultBindSet );			
			}
			pass->updateDescriptorSets = true;
		}
	}

	for ( uint32_t i = 0; i < MaxFrameStates; ++i )
	{
		particleState.parms[ i ] = RegisterBindParm( &particleShaderBinds );
		particleState.updateDescriptorSets = true;
	}

	materialBuffer.Reset();

	AllocRegisteredBindParms();

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
	vkInfo.MinImageCount = MaxFrameStates;
	vkInfo.ImageCount = MaxFrameStates;
	vkInfo.CheckVkResultFn = nullptr;

	assert( view.passes[ DRAWPASS_POST_2D ] != nullptr );
	const renderPassTransitionFlags_t transitionState = view.passes[ DRAWPASS_POST_2D ]->transitionState;
	ImGui_ImplVulkan_Init( &vkInfo, view.passes[ DRAWPASS_POST_2D ]->fb->GetVkRenderPass( transitionState ) );

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Upload Fonts
	{
		BeginUploadCommands( uploadContext );
		VkCommandBuffer commandBuffer = uploadContext.CommandBuffer();
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

	// Debug Markers
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties( context.physicalDevice, nullptr, &extensionCount, nullptr );

		std::vector<VkExtensionProperties> availableExtensions( extensionCount );
		vkEnumerateDeviceExtensionProperties( context.physicalDevice, nullptr, &extensionCount, availableExtensions.data() );

		bool found = false;
		for ( const auto& extension : availableExtensions ) {
			if ( strcmp( extension.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				found = true;
				break;
			}
		}

		if ( found )
		{
			std::cout << "Enabling debug markers." << std::endl;

			context.fnDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr( context.device, "vkDebugMarkerSetObjectTagEXT" );
			context.fnDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr( context.device, "vkDebugMarkerSetObjectNameEXT" );
			context.fnCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr( context.device, "vkCmdDebugMarkerBeginEXT" );
			context.fnCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr( context.device, "vkCmdDebugMarkerEndEXT" );
			context.fnCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr( context.device, "vkCmdDebugMarkerInsertEXT" );

			context.debugMarkersEnabled = ( context.fnDebugMarkerSetObjectName != VK_NULL_HANDLE );
		}
		else {
			std::cout << "Debug markers \"" << VK_EXT_DEBUG_MARKER_EXTENSION_NAME << "\" disabled." << std::endl;
		}
	}

	// Default Bilinear Sampler
	{	
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

		if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &context.bilinearSampler ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create texture sampler!" );
		}
	}

	// Depth sampler
	{		
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

		if ( vkCreateSampler( context.device, &samplerInfo, nullptr, &context.depthShadowSampler ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create depth sampler!" );
		}
	}

	context.bufferId = 0;
}


void Renderer::BuildPipelines()
{
	const uint32_t programCount = g_assets.gpuPrograms.Count();

	std::vector< Asset<GpuProgram>* > invalidAssets;
	invalidAssets.reserve( programCount );

	// 1. Collect stale shaders	
	for ( uint32_t progIx = 0; progIx < programCount; ++progIx )
	{
		Asset<GpuProgram>* progAsset = g_assets.gpuPrograms.Find( progIx );
		if ( progAsset == nullptr ) {
			continue;
		}

		if ( progAsset->IsUploaded() ) {
			continue;
		}
		invalidAssets.push_back( progAsset );
	}

	if( invalidAssets.size() == 0 ) {
		return;
	}

	// 2. Destroy shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();
		for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx ) {
			if ( prog.vk_shaders[ shaderIx ] != VK_NULL_HANDLE ) {
				vkDestroyShaderModule( context.device, prog.vk_shaders[ shaderIx ], nullptr );
			}
		}
	}

	// 3. Create shaders
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		GpuProgram& prog = progAsset->Get();
		for ( uint32_t shaderIx = 0; shaderIx < prog.shaderCount; ++shaderIx ) {
			prog.vk_shaders[ shaderIx ] = vk_CreateShaderModule( prog.shaders[ shaderIx ].blob );
		}
	}

	// 4. Collect all passes in active views
	std::vector<const DrawPass*> passes;
	passes.reserve( MaxViews * DRAWPASS_COUNT );

	for ( uint32_t viewIx = 0; viewIx < MaxViews; ++viewIx )
	{
		if( views[ viewIx ].IsCommitted() == false ) {
			continue;
		}

		for ( int passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			const DrawPass* pass = views[ viewIx ].passes[ passIx ];
			if( pass != nullptr ) {
				passes.push_back( pass );
			}
		}
	}

	// 5. Destroy pipelines. Own pass so cache isn't destroyed per iteration
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		progAsset->CompleteUpload();

		GpuProgram& prog = progAsset->Get();
		if ( prog.shaders[ 0 ].type == shaderType_t::COMPUTE )
		{
			assert( prog.shaderCount == 1 );
			DestroyComputePipeline( *progAsset );
			continue;
		}

		const uint32_t passCount = static_cast<uint32_t>( passes.size() );
		for ( uint32_t passIx = 0; passIx < passCount; ++passIx ) {
			DestroyGraphicsPipeline( passes[ passIx ], *progAsset );
		}
	}

	// 6. Create pipelines
	for ( auto it = invalidAssets.begin(); it != invalidAssets.end(); ++it )
	{
		Asset<GpuProgram>* progAsset = *it;
		progAsset->CompleteUpload();

		GpuProgram& prog = progAsset->Get();
		if ( prog.shaders[ 0 ].type == shaderType_t::COMPUTE )
		{
			assert( prog.shaderCount == 1 );
			CreateComputePipeline( *progAsset );
			continue;
		}

		const uint32_t passCount = static_cast<uint32_t>( passes.size() );
		for ( uint32_t passIx = 0; passIx < passCount; ++passIx ) {
			CreateGraphicsPipeline( passes[ passIx ], *progAsset );
		}	
	}
}


void Renderer::CreateTempCanvas( const imageInfo_t& info, const renderViewRegion_t region )
{
	// Image resource
	{
		CreateImage( "tempColor", info, GPU_IMAGE_READ, frameBufferMemory, tempColorImage );
	}

	// Frame buffer
	{
		frameBufferCreateInfo_t fbInfo = {};
		fbInfo.color0[ 0 ] = &tempColorImage;
		fbInfo.width = tempColorImage.info.width;
		fbInfo.height = tempColorImage.info.height;
		fbInfo.lifetime = LIFETIME_TEMP;

		tempColor.Create( fbInfo );
	}

	// Frame buffer
	{
		view2Ds[ 1 ] = &views[ MaxViews - 1 ];
		view2Ds[ 1 ]->Init( "Temp Canvas", region, MaxViews - 1, tempColor );
		view2Ds[ 1 ]->Commit();
	}
}


void Renderer::CreateFramebuffers()
{
	int width = 0;
	int height = 0;
	g_window.GetWindowFrameBufferSize( width, height );

	// Shadow images
	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
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

		CreateImage( "shadowMap", info, GPU_IMAGE_READ, frameBufferMemory, shadowMapImage[ shadowIx ] );
	}

	// Main images
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

		CreateImage( "mainColor", info, GPU_IMAGE_READ, frameBufferMemory, mainColorImage );

		info.fmt = IMAGE_FMT_D_32_S8;
		info.type = IMAGE_TYPE_2D;
		info.aspect = imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG );

		CreateImage( "viewDepth", info, GPU_IMAGE_READ, frameBufferMemory, depthStencilImage );
	}

	{
		imageInfo_t depthInfo = depthStencilImage.info;
		depthInfo.aspect = IMAGE_ASPECT_DEPTH_FLAG;
		frameState.depthImageView.Init( depthStencilImage, depthInfo );

		imageInfo_t stencilInfo = depthStencilImage.info;
		stencilInfo.aspect = IMAGE_ASPECT_STENCIL_FLAG;
		frameState.stencilImageView.Init( depthStencilImage, stencilInfo );
	}

	// Shadow map
	for ( uint32_t shadowIx = 0; shadowIx < MaxShadowMaps; ++shadowIx )
	{	
		frameBufferCreateInfo_t fbInfo = {};
		fbInfo.depth[ 0 ] = &shadowMapImage[ shadowIx ];
		fbInfo.width = shadowMapImage[ shadowIx ].info.width;
		fbInfo.height = shadowMapImage[ shadowIx ].info.height;
		fbInfo.lifetime = LIFETIME_TEMP;

		shadowMap[ shadowIx ].Create( fbInfo );
	}

	// Main Scene 3D Render
	{
		frameBufferCreateInfo_t fbInfo = {};
		for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
		{
			fbInfo.color0[ frameIx ] = &mainColorImage;
			fbInfo.depth[ frameIx ] = &frameState.depthImageView;
			fbInfo.stencil[ frameIx ] = &frameState.stencilImageView;
		}
		fbInfo.width = mainColorImage.info.width;
		fbInfo.height = mainColorImage.info.height;
		fbInfo.lifetime = LIFETIME_TEMP;

		mainColor.Create( fbInfo );
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

	bindParmsList.Append( parms );

	return &bindParmsList[ bindParmsList.Count() - 1 ];
}


void Renderer::AllocRegisteredBindParms()
{
	std::vector<VkDescriptorSetLayout> layouts;
	std::vector<VkDescriptorSet> descSets;

	const uint32_t bindParmCount = bindParmsList.Count();

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
	descSets.reserve( bindParmsList.Count() );

	for ( uint32_t i = 0; i < bindParmsList.Count(); ++i )
	{
		ShaderBindParms& parms = bindParmsList[ i ];
		descSets.push_back( parms.GetVkObject() );
	}

	vkFreeDescriptorSets( context.device, descriptorPool, static_cast<uint32_t>( descSets.size() ), descSets.data() );
}


void Renderer::CreateBuffers()
{
	{
		frameState.globalConstants.Create( "Globals", LIFETIME_PERSISTENT, 1, sizeof( viewBufferObject_t ), bufferType_t::UNIFORM, sharedMemory );
		frameState.viewParms.Create( "View", LIFETIME_PERSISTENT, MaxViews, sizeof( viewBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState.surfParms.Create( "Surf", LIFETIME_PERSISTENT, MaxViews * MaxSurfaces, sizeof( uniformBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState.materialBuffers.Create( "Material", LIFETIME_PERSISTENT, MaxMaterials, sizeof( materialBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState.lightParms.Create( "Light", LIFETIME_PERSISTENT, MaxLights, sizeof( lightBufferObject_t ), bufferType_t::STORAGE, sharedMemory );
		frameState.particleBuffer.Create( "Particle", LIFETIME_PERSISTENT, MaxParticles, sizeof( particleBufferObject_t ), bufferType_t::STORAGE, sharedMemory );

		for ( size_t v = 0; v < MaxViews; ++v ) {
			frameState.surfParmPartitions[ v ] = frameState.surfParms.GetView( v * MaxSurfaces, MaxSurfaces );
		}
	}

	vb.Create( "VB", LIFETIME_TEMP, MaxVertices, sizeof( vsInput_t ), bufferType_t::VERTEX, localMemory );
	ib.Create( "IB", LIFETIME_TEMP, MaxIndices, sizeof( uint32_t ), bufferType_t::INDEX, localMemory );

	stagingBuffer.Create( "Staging", LIFETIME_TEMP, 1, 256 * MB_1, bufferType_t::STAGING, sharedMemory );
}


void Renderer::CreateImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage )
{
	outImage.info = info;
	outImage.gpuImage = new GpuImage();
	outImage.gpuImage->Create( name, outImage.info, flags, memory );
}


void Renderer::CreateCodeTextures() {
	imageInfo_t info{};
	info.width = 256;
	info.height = 256;
	info.mipLevels = 1;
	info.layers = 1;
	info.subsamples = IMAGE_SMP_1;
	info.fmt = IMAGE_FMT_BGRA_8;
	info.aspect = IMAGE_ASPECT_COLOR_FLAG;
	info.type = IMAGE_TYPE_2D;
	info.tiling = IMAGE_TILING_MORTON;

	// Default Images
	CreateImage( "_white", info, GPU_IMAGE_READ, localMemory, rc.whiteImage );
	CreateImage( "_black", info, GPU_IMAGE_READ, localMemory, rc.blackImage );
}


void Renderer::CreateSyncObjects()
{
	gfxContext.presentSemaphore.Create();
	gfxContext.renderFinishedSemaphore.Create();
	computeContext.semaphore.Create();

	uploadFinishedSemaphore.Create();

	for ( size_t i = 0; i < MaxFrameStates; ++i ) {
		gfxContext.frameFence[ i ].Create();
	}

#ifdef USE_VULKAN
	gfxContext.presentSemaphore.waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif
}