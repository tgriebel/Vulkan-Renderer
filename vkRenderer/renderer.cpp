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