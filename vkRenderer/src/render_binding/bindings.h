#pragma once

#include "shaderBinding.h"

extern ShaderBinding	bind_globalsBuffer;

// Compute Resources
extern ShaderBinding	bind_particleWriteBuffer;

// Raster Resources
extern ShaderBinding	bind_viewBuffer;
extern ShaderBinding	bind_modelBuffer;
extern ShaderBinding	bind_image2DArray;
extern ShaderBinding	bind_imageCubeArray;
extern ShaderBinding	bind_materialBuffer;
extern ShaderBinding	bind_lightBuffer;
extern ShaderBinding	bind_imageCodeArray;
extern ShaderBinding	bind_imageStencil;
extern ShaderBinding	bind_sourceImages;
extern ShaderBinding	bind_imageProcess;

const uint32_t g_defaultBindCount = 9;
extern const ShaderBinding g_defaultBindings[ g_defaultBindCount ];

const uint32_t g_particleBindCount = 2;
extern const ShaderBinding g_particleBindings[ g_particleBindCount ];

const uint32_t g_imageProcessBindCount = 3;
extern const ShaderBinding g_imageProcessBindings[ g_imageProcessBindCount ];