#pragma once

#include "shaderBinding.h"

#define BINDING( NAME, TYPE, COUNT, FLAGS )	static const ShaderBinding bind_##NAME( #NAME, bindType_t::TYPE, COUNT, FLAGS )

BINDING( globalsBuffer, CONSTANT_BUFFER, 1, BIND_STATE_ALL );

// Compute Resources
BINDING( particleWriteBuffer, WRITE_BUFFER, 1, BIND_STATE_CS );

// Post Effect Resources
BINDING( imageProcess,		CONSTANT_BUFFER,	1,						BIND_STATE_PS );
BINDING( sourceImages,		IMAGE_2D_ARRAY,		MaxCodeImages,			BIND_STATE_PS );

// Raster Resources
BINDING( viewBuffer,		READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( modelBuffer,		READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( image2DArray,		IMAGE_2D_ARRAY,		MaxImageDescriptors,	BIND_STATE_ALL );
BINDING( imageCubeArray,	IMAGE_CUBE_ARRAY,	MaxImageDescriptors,	BIND_STATE_ALL );
BINDING( materialBuffer,	READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( lightBuffer,		READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( imageCodeArray,	IMAGE_2D_ARRAY,		MaxCodeImages,			BIND_STATE_ALL );
BINDING( imageStencil,		IMAGE_2D,			1,						BIND_STATE_ALL );


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
	bind_modelBuffer,
	bind_imageCodeArray,
	bind_imageStencil
};
const uint64_t bindset_pass = Hash( "bindset_pass" );


static const ShaderBinding g_particleBindings[] =
{
	bind_globalsBuffer,
	bind_particleWriteBuffer
};
const uint64_t bindset_particle = Hash( "bindset_particle" );


static const ShaderBinding g_imageProcessBindings[] =
{
	bind_sourceImages,
	bind_imageStencil,
	bind_imageProcess,
};
const uint64_t bindset_imageProcess = Hash( "bindset_imageProcess" );