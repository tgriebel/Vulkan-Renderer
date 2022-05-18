#include <algorithm>
#include <iterator>
#include <map>
#include "renderer.h"

static bool CompareSortKey( drawSurf_t& surf0, drawSurf_t& surf1 )
{
	if( surf0.materialId == surf1.materialId ) {
		return ( surf0.objectId < surf1.objectId );
	} else {
		return ( surf0.materialId < surf1.materialId );
	}
}

void Renderer::Commit( const Scene& scene )
{
	renderView.committedModelCnt = 0;
	for ( uint32_t i = 0; i < scene.entities.Count(); ++i ) {
		CommitModel( renderView, **scene.entities.Find( i ), 0 );
	}
	MergeSurfaces( renderView );

	shadowView.committedModelCnt = 0;
	for ( uint32_t i = 0; i < scene.entities.Count(); ++i )
	{
		if ( ( *scene.entities.Find( i ) )->HasRenderFlag( NO_SHADOWS ) ) {
			continue;
		}
		CommitModel( shadowView, **scene.entities.Find( i ), MaxModels );
	}
	MergeSurfaces( shadowView );

	renderView.viewMatrix = scene.camera.GetViewMatrix();
	renderView.projMatrix = scene.camera.GetPerspectiveMatrix();

	for ( int i = 0; i < MaxLights; ++i ) {
		renderView.lights[ i ] = scene.lights[ i ];
	}

	UpdateView();
}

void Renderer::UploadModelsToGPU()
{
	const VkDeviceSize vbSize = sizeof( VertexInput ) * MaxVertices;
	const VkDeviceSize ibSize = sizeof( uint32_t ) * MaxIndices;
	const uint32_t modelCount = modelLib.Count();
	CreateBuffer( vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vb, localMemory );
	CreateBuffer( ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ib, localMemory );

	static uint32_t vbBufElements = 0;
	static uint32_t ibBufElements = 0;
	for ( uint32_t m = 0; m < modelCount; ++m )
	{
		modelSource_t* model = modelLib.Find( m );
		for ( uint32_t s = 0; s < model->surfCount; ++s )
		{
			surface_t& surf = model->surfs[ s ];
			surfUpload_t& upload = model->upload[ s ];
			VkDeviceSize vbCopySize = sizeof( surf.vertices[ 0 ] ) * surf.vertices.size();
			VkDeviceSize ibCopySize = sizeof( surf.indices[ 0 ] ) * surf.indices.size();

			upload.vertexOffset = vbBufElements;
			upload.firstIndex = ibBufElements;

			// VB Copy
			stagingBuffer.Reset();
			stagingBuffer.CopyData( surf.vertices.data(), static_cast<size_t>( vbCopySize ) );

			VkBufferCopy vbCopyRegion{ };
			vbCopyRegion.size = vbCopySize;
			vbCopyRegion.srcOffset = 0;
			vbCopyRegion.dstOffset = vb.GetSize();
			CopyGpuBuffer( stagingBuffer, vb, vbCopyRegion );

			const uint32_t vertexCount = static_cast<uint32_t>( surf.vertices.size() );
			upload.vertexCount = vertexCount;
			vbBufElements += vertexCount;

			// IB Copy
			stagingBuffer.Reset();
			stagingBuffer.CopyData( surf.indices.data(), static_cast<size_t>( ibCopySize ) );

			VkBufferCopy ibCopyRegion{ };
			ibCopyRegion.size = ibCopySize;
			ibCopyRegion.srcOffset = 0;
			ibCopyRegion.dstOffset = ib.GetSize();
			CopyGpuBuffer( stagingBuffer, ib, ibCopyRegion );

			const uint32_t indexCount = static_cast<uint32_t>( surf.indices.size() );
			upload.indexCount = indexCount;
			ibBufElements += indexCount;
		}
	}
}

void Renderer::MergeSurfaces( RenderView& view )
{
	view.mergedModelCnt = 0;
	std::unordered_map< drawSurf_t, uint32_t > uniqueSurfs;
	uniqueSurfs.reserve( view.committedModelCnt );
	for ( uint32_t i = 0; i < view.committedModelCnt; ++i ) {
		drawSurfInstance_t& instance = view.instances[ i ];
		auto it = uniqueSurfs.find( view.surfaces[ i ] );
		if ( it == uniqueSurfs.end() ) {
			const uint32_t surfId = view.mergedModelCnt;
			uniqueSurfs[ view.surfaces[ i ] ] = surfId;

			view.instanceCounts[ surfId ] = 1;
			view.merged[ surfId ] = view.surfaces[ i ];

			instance.id = 0;
			instance.surfId = surfId;

			++view.mergedModelCnt;
		}
		else {
			instance.id = view.instanceCounts[ it->second ];
			instance.surfId = it->second;
			view.instanceCounts[ it->second ]++;
		}
	}
	uint32_t totalCount = 0;
	for ( uint32_t i = 0; i < view.mergedModelCnt; ++i ) {
		view.merged[ i ].objectId += totalCount;
		totalCount += view.instanceCounts[ i ];
	}
}

void Renderer::CommitModel( RenderView& view, const Entity& ent, const uint32_t objectOffset )
{
	if ( ent.HasRenderFlag( HIDDEN ) ) {
		return;
	}

	modelSource_t* source = modelLib.Find( ent.modelId );
	for ( uint32_t i = 0; i < source->surfCount; ++i ) {
		drawSurfInstance_t& instance = view.instances[ view.committedModelCnt ];
		drawSurf_t& surf = view.surfaces[ view.committedModelCnt ];
		surfUpload_t& upload = source->upload[ i ];

		instance.modelMatrix = ent.GetMatrix();
		instance.surfId = 0;
		instance.id = 0;
		surf.vertexCount = upload.vertexCount;
		surf.vertexOffset = upload.vertexOffset;
		surf.firstIndex = upload.firstIndex;
		surf.indicesCnt = upload.indexCount;
		surf.materialId = ( ent.materialId >= 0 ) ? ent.materialId : source->surfs[ i ].materialId;
		surf.objectId = objectOffset;
		surf.flags = ent.GetRenderFlags();
		surf.stencilBit = ent.outline ? 0x01 : 0;

		for ( int pass = 0; pass < DRAWPASS_COUNT; ++pass ) {
			const Material* material = materialLib.Find( surf.materialId );
			if ( material->shaders[ pass ].IsValid() ) {
				GpuProgram* prog = gpuPrograms.Find( material->shaders[ pass ].Get() );
				if ( prog == nullptr ) {
					continue;
				}
				surf.pipelineObject[ pass ] = prog->pipeline;
			}
			else {
				surf.pipelineObject[ pass ] = INVALID_HANDLE;
			}
		}

		++view.committedModelCnt;
	}
}

gfxStateBits_t Renderer::GetStateBitsForDrawPass( const drawPass_t pass )
{
	uint64_t stateBits = 0;
	if ( pass == DRAWPASS_SKYBOX )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
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
		stateBits |= GFX_STATE_MSAA_ENABLE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_STENCIL_ENABLE;
	}
	else if ( pass == DRAWPASS_TERRAIN )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}
	else if ( pass == DRAWPASS_OPAQUE )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_DEPTH_WRITE;
		stateBits |= GFX_STATE_MSAA_ENABLE;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
	}
	else if ( pass == DRAWPASS_TRANS )
	{
		stateBits |= GFX_STATE_DEPTH_TEST;
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_MSAA_ENABLE;
		stateBits |= GFX_STATE_BLEND_ENABLE;
	}
	else if ( pass == DRAWPASS_DEBUG_WIREFRAME )
	{
		stateBits |= GFX_STATE_WIREFRAME_ENABLE;
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}
	else if ( pass == DRAWPASS_DEBUG_SOLID )
	{
		stateBits |= GFX_STATE_CULL_MODE_BACK;
		stateBits |= GFX_STATE_BLEND_ENABLE;
		stateBits |= GFX_STATE_MSAA_ENABLE;
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
		stateBits |= GFX_STATE_MSAA_ENABLE;
	}

	return static_cast<gfxStateBits_t>( stateBits );
}

viewport_t Renderer::GetDrawPassViewport( const drawPass_t pass )
{
	if ( pass == DRAWPASS_SHADOW ) {
		return shadowView.viewport;
	}
	else {
		return renderView.viewport;
	}
}

void Renderer::CreateLogicalDevice()
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

void Renderer::PickPhysicalDevice()
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
			msaaSamples = GetMaxUsableSampleCount();
			break;
		}
	}

	if ( context.physicalDevice == VK_NULL_HANDLE ) {
		throw std::runtime_error( "Failed to find a suitable GPU!" );
	}
}

std::vector<const char*> Renderer::GetRequiredExtensions() const
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

	if ( enableValidationLayers ) {
		extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	}

	return extensions;
}

bool Renderer::CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

	std::vector<VkLayerProperties> availableLayers( layerCount );
	vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

	for ( const char* layerName : validationLayers ) {
		bool layerFound = false;
		for ( const auto& layerProperties : availableLayers ) {
			if ( strcmp( layerName, layerProperties.layerName ) == 0 ) {
				layerFound = true;
				break;
			}
		}
		if ( !layerFound ) {
			return false;
		}
	}
	return true;
}

void Renderer::PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
{
	createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = 0;
	if ( ValidateVerbose ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	}

	if ( ValidateWarnings ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	if ( ValidateErrors ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
}

void Renderer::SetupDebugMessenger()
{
	if ( !enableValidationLayers ) {
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo( createInfo );

	if ( CreateDebugUtilsMessengerEXT( context.instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS )
	{
		throw std::runtime_error( "Failed to set up debug messenger!" );
	}
}

VkResult Renderer::CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void Renderer::DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		func( instance, debugMessenger, pAllocator );
	}
}