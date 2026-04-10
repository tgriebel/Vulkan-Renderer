#pragma once

#include "shaderBinding.h"

// *********************** IMPORTANT ***********************
// 
// Must mirror changes and recompile all shaders when adjusting bindings
//
// *********************************************************

#define BINDING( NAME, TYPE, COUNT, FLAGS )	static const ShaderBinding bind_##NAME( #NAME, bindType_t::TYPE, COUNT, FLAGS )

BINDING( globalsBuffer, CONSTANT_BUFFER, 1, BIND_STATE_ALL );

// Compute Resources
BINDING( particleWriteBuffer,	WRITE_BUFFER,		1,						BIND_STATE_CS );
BINDING( computeParms,			CONSTANT_BUFFER,	1,						BIND_STATE_CS );
BINDING( computeWrite,			WRITE_BUFFER,		1,						BIND_STATE_CS );
BINDING( computeImage,			IMAGE_2D_ARRAY,		MaxImageDescriptors,	BIND_STATE_CS );

// Post Effect Resources
BINDING( imageProcess,			CONSTANT_BUFFER,	1,						BIND_STATE_PS );
BINDING( sourceImages,			IMAGE_2D_ARRAY,		3,						BIND_STATE_PS );
BINDING( sourceCubeImages,		IMAGE_CUBE,			1,						BIND_STATE_PS );

// Raster Resources
BINDING( viewBuffer,			READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( modelBuffer,			READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( image2DArray,			IMAGE_2D_ARRAY,		MaxImageDescriptors,	BIND_STATE_ALL_GFX );
BINDING( imageCubeArray,		IMAGE_CUBE_ARRAY,	MaxImageDescriptors,	BIND_STATE_ALL_GFX );
BINDING( materialBuffer,		READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( lightBuffer,			READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( passBuffer,			READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( imageCodeArray,		IMAGE_2D_ARRAY,		MaxCodeImages,			BIND_STATE_ALL_GFX );
BINDING( imageCodeCubeArray,	IMAGE_CUBE_ARRAY,	MaxCodeImages,			BIND_STATE_ALL_GFX );
BINDING( imageStencil,			IMAGE_2D,			1,						BIND_STATE_ALL_GFX );
BINDING( bilinearWrapSampler,	IMAGE_SAMPLER,		1,						BIND_STATE_ALL_GFX );
BINDING( bilinearClampSampler,	IMAGE_SAMPLER,		1,						BIND_STATE_ALL_GFX );


static const ShaderBinding g_globalBindings[] =
{
	bind_globalsBuffer,
	bind_viewBuffer,
	bind_image2DArray,
	bind_imageCubeArray,
	bind_materialBuffer,
};
const uint64_t bindset_global = Hash( "bindset_global" );


static const ShaderBinding g_viewBindings[] =
{
	bind_modelBuffer,
};
const uint64_t bindset_view = Hash( "bindset_view" );


static const ShaderBinding g_passBindings[] =
{
	bind_lightBuffer,
	bind_imageCodeArray,
	bind_imageCodeCubeArray,
	bind_imageStencil
};
const uint64_t bindset_pass = Hash( "bindset_pass" );


static const ShaderBinding g_particleBindings[] =
{
	bind_globalsBuffer,
	bind_particleWriteBuffer
};
const uint64_t bindset_particle = Hash( "bindset_particle" );


static const ShaderBinding g_computeBindings[] =
{
	bind_globalsBuffer,
	bind_computeImage,
	bind_computeParms,
	bind_computeWrite,
};
const uint64_t bindset_compute = Hash( "bindset_compute" );


static const ShaderBinding g_imageProcessBindings[] =
{
	bind_sourceImages,
	bind_sourceCubeImages,
	bind_imageStencil,
	bind_imageProcess,
};
const uint64_t bindset_imageProcess = Hash( "bindset_imageProcess" );