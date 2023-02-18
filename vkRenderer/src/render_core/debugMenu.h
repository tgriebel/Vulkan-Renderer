#pragma once

class Scene;

#if defined( USE_IMGUI )
void DebugMenuMaterial( const Material& mat );
void DebugMenuMaterialEdit( Asset<Material>* matAsset );
void DebugMenuModelTreeNode( Asset<Model>* modelAsset );
void DebugMenuTextureTreeNode( Asset<Texture>* texAsset );
void DebugMenuLightEdit( Scene* scene );
void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties );
#endif