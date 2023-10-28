#pragma once

class Scene;

struct renderDebugData_t
{
	uint32_t	frameNumber;
	float		frameTimeMs;
	float		mouseX;
	float		mouseY;
};

extern renderDebugData_t g_renderDebugData;

#if defined( USE_IMGUI )
void DebugMenuMaterial( const Material& mat );
void DebugMenuMaterialEdit( Asset<Material>* matAsset );
void DebugMenuModelTreeNode( Asset<Model>* modelAsset );
void DebugMenuTextureTreeNode( Asset<Image>* texAsset );
void DebugMenuShaderTreeNode( Asset<GpuProgram>* shaderAsset );
void DebugMenuLightEdit( Scene* scene );
void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties, VkPhysicalDeviceFeatures deviceFeatures );
void DeviceDebugMenu();
#endif