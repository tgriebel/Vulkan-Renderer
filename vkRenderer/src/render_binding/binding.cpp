#include "bindings.h"
#include "../globals/common.h"

#define BINDING( NAME, TYPE, COUNT, FLAGS )	ShaderBinding bind_##NAME( #NAME, TYPE, COUNT, FLAGS )

BINDING( globalsBuffer, CONSTANT_BUFFER, 1, BIND_STATE_ALL );

// Compute Resources
BINDING( particleWriteBuffer, WRITE_BUFFER, 1, BIND_STATE_CS );

// Raster Resources
BINDING( viewBuffer, READ_BUFFER, 1, BIND_STATE_ALL );
BINDING( modelBuffer, READ_BUFFER, 1, BIND_STATE_ALL );
BINDING( image2DArray, IMAGE_2D_ARRAY, MaxImageDescriptors, BIND_STATE_ALL );
BINDING( imageCubeArray, IMAGE_CUBE_ARRAY, MaxImageDescriptors, BIND_STATE_ALL );
BINDING( materialBuffer, READ_BUFFER, 1, BIND_STATE_ALL );
BINDING( lightBuffer, READ_BUFFER, 1, BIND_STATE_ALL );
BINDING( imageCodeArray, IMAGE_2D_ARRAY, MaxCodeImages, BIND_STATE_ALL );
BINDING( imageStencil, IMAGE_2D, 1, BIND_STATE_ALL );


const ShaderBinding g_defaultBindings[ g_defaultBindCount ] =
											{	
												bind_globalsBuffer,
												bind_viewBuffer,
												bind_modelBuffer,
												bind_image2DArray,
												bind_imageCubeArray,
												bind_materialBuffer,
												bind_lightBuffer,
												bind_imageCodeArray,
												bind_imageStencil
											};

const ShaderBinding g_particleCsBindings[ g_particleCsBindCount ] =
											{
												bind_globalsBuffer,
												bind_particleWriteBuffer 
											};