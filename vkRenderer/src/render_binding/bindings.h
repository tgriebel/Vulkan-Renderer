#pragma once

#include "pipeline.h"

ShaderBinding	bind_globalsBuffer( 0, CONSTANT_BUFFER, 1, BIND_STATE_ALL );

// Compute Resources
ShaderBinding	bind_particleWriteBuffer( 1, WRITE_BUFFER, 1, BIND_STATE_CS );

// Raster Resources
ShaderBinding	bind_viewBuffer( 1, READ_BUFFER, 1, BIND_STATE_ALL );
ShaderBinding	bind_modelBuffer( 2, READ_BUFFER, 1, BIND_STATE_ALL );
ShaderBinding	bind_image2DArray( 3, IMAGE_2D, MaxImageDescriptors, BIND_STATE_ALL );
ShaderBinding	bind_imageCubeArray( 4, IMAGE_CUBE, MaxImageDescriptors, BIND_STATE_ALL );
ShaderBinding	bind_materialBuffer( 5, READ_BUFFER, 1, BIND_STATE_ALL );
ShaderBinding	bind_lightBuffer( 6, READ_BUFFER, 1, BIND_STATE_ALL );
ShaderBinding	bind_imageCodeArray( 7, IMAGE_2D, MaxCodeImages, BIND_STATE_ALL );
ShaderBinding	bind_imageStencil( 8, IMAGE_2D, 1, BIND_STATE_ALL );