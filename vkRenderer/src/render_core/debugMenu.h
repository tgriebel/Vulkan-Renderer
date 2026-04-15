#pragma once

class Scene;
class Model;

template<class AssetType>
class Asset;

struct renderDebugData_t
{
	uint64_t	frameNumber;
	float		frameTimeMs;
	float		mouseX;
	float		mouseY;
};

extern renderDebugData_t g_renderDebugData;

const char* FormatByteSize( const uint64_t bytes );

#if defined( USE_IMGUI )
void DebugMenuMaterial( const Material& mat );
void DebugMenuMaterialEdit( Asset<Material>* matAsset );
void DebugMenuModelTreeNode( Asset<Model>* modelAsset );
void DebugMenuTextureTreeNode( Asset<Image>* texAsset );
void DebugMenuShaderTreeNode( Asset<GpuProgram>* shaderAsset );
void DebugMenuEntityEdit( Scene* scene );
#ifdef USE_VULKAN
void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties, VkPhysicalDeviceFeatures2 deviceFeatures );
#endif
void DeviceDebugMenu();
#endif
