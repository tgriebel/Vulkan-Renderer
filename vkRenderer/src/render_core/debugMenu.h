#pragma once

#if defined( USE_IMGUI )
void DebugMenuMaterial( const Material& mat );
void DebugMenuMaterialEdit( Asset<Material>* matAsset );
void DebugMenuModelTreeNode( Asset<Model>* modelAsset );
void DebugMenuTextureTreeNode( Asset<Texture>* texAsset );
void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties );
#endif