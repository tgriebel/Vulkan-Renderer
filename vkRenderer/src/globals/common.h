/*
* MIT License
* 
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
* 
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define USE_VULKAN
#define USE_IMGUI

#include <gfxcore/acceleration/aabb.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <utility>
#include <set>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>
#include <ctime>
#include <ratio>
#include <unordered_map>
#include <stdlib.h>
#include <math.h>
#include <atomic>

#include <gfxcore/math/vector.h>
#include <gfxcore/core/handle.h>
#include <gfxcore/image/color.h>
#include <gfxcore/asset_types/material.h>
#include <gfxcore/primitives/geom.h>
#include <gfxcore/scene/camera.h>
#include <gfxcore/asset_types/gpuProgram.h>
#include <syscore/common.h>
#include <sysCore/array.h>
#include <syscore/timer.h>

#ifdef NDEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = true;
#endif

const bool		ValidateVerbose					= false;
const bool		ValidateWarnings				= false;
const bool		ValidateErrors					= true;

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1000;
const uint32_t	DescriptorPoolMaxSamplers		= 3;
const uint32_t	DescriptorPoolMaxImages			= 1000;
const uint32_t	DescriptorPoolMaxComboImages	= 1000;
const uint32_t	DescriptorPoolMaxSets			= 512;
const uint32_t	MaxImageDescriptors				= 100;
const uint32_t	MaxPostImageDescriptors			= 3;
const uint32_t	MaxUboDescriptors				= 4;
const uint32_t	MaxLights						= 128;
const uint32_t	MaxParticles					= 1024;
const uint32_t	MaxShadowMaps					= 6;
const uint32_t	MaxShadowViews					= MaxShadowMaps;
const uint32_t	Max2DViews						= 2;
const uint32_t	Max3DViews						= 4;
const uint32_t	MaxViews						= ( MaxShadowViews + Max3DViews + Max2DViews );
const uint32_t	MaxModels						= 1000;
const uint32_t	MaxVertices						= 0x000FFFFF;
const uint32_t	MaxIndices						= 0x000FFFFF;
const uint32_t	MaxSurfaces						= MaxModels;
const uint32_t	MaxSurfacesDescriptors			= 1;
const uint32_t	MaxMaterials					= 256;
const uint32_t	MaxCodeImages					= 3;
const uint64_t	MaxSharedMemory					= MB( 1024 );
const uint64_t	MaxLocalMemory					= MB( 1024 );
const uint64_t	MaxFrameBufferMemory			= GB( 2 );
const uint64_t	MaxFrameStates					= 3;
const uint64_t	MaxTimeStampQueries				= 12;
const uint64_t	MaxOcclusionQueries				= 12;
const uint32_t	DefaultDisplayWidth				= 1280;
const uint32_t	DefaultDisplayHeight			= 720;

const std::string ModelPath = ".\\models\\";
const std::string TexturePath = ".\\textures\\";
const std::string MaterialPath = ".\\materials\\";
const std::string BakePath = ".\\baked\\";
const std::string BakedModelExtension = ".mdl.bin";
const std::string BakedTextureExtension = ".img.bin";
const std::string BakedMaterialExtension = ".mtl.bin";

uint32_t Hash( const uint8_t* bytes, const uint32_t sizeBytes );

using ImageArray = Array<Image*, MaxImageDescriptors>;

class Renderer;
class Serializer;

struct vsInput_t
{
	vec3f pos;
	vec4f color;
	vec3f normal;
	vec3f tangent;
	vec3f bitangent;
	vec4f texCoord;
};


enum renderFlags_t
{
	NONE		= 0,
	HIDDEN		= ( 1 << 0 ),
	NO_SHADOWS	= ( 1 << 1 ),
	WIREFRAME	= ( 1 << 2 ),	// FIXME: this needs to make a unique pipeline object if checked
	DEBUG_SOLID	= ( 1 << 3 ),	// "
	SKIP_OPAQUE	= ( 1 << 4 ),
	COMMITTED	= ( 1 << 5 ),
};


struct pipelineObject_t;

union sortKey_t
{
	uint32_t	materialId : 32;
	uint32_t	key;
};


struct drawSurf_t
{
	uint32_t			uploadId;
	uint32_t			objectId;
	sortKey_t			sortKey;
	renderFlags_t		flags;
	uint8_t				stencilBit;
	uint32_t			hash;

	const char*			dbgName;

	hdl_t				pipelineObject[ DRAWPASS_COUNT ];
};


inline uint32_t Hash( const drawSurf_t& surf ) {
	uint64_t shaderIds[ DRAWPASS_COUNT ];
	for ( uint32_t i = 0; i < DRAWPASS_COUNT; ++i ) {
		shaderIds[ i ] = surf.pipelineObject[ i ].Get();
	}
	uint32_t shaderHash = Hash( reinterpret_cast<const uint8_t*>( &shaderIds ), sizeof( shaderIds[ 0 ] ) * DRAWPASS_COUNT );
	uint32_t stateHash = Hash( reinterpret_cast<const uint8_t*>( &surf ), offsetof( drawSurf_t, hash ) );
	return ( shaderHash ^ stateHash );
}


struct drawSurfInstance_t
{
	mat4x4f		modelMatrix;
	uint32_t	surfId;
	uint32_t	id;
};


inline bool operator==( const drawSurf_t& lhs, const drawSurf_t& rhs )
{
	bool isEqual = true;
	const uint32_t sizeBytes = sizeof( drawSurf_t );
	const uint8_t* lhsBytes = reinterpret_cast< const uint8_t* >( &lhs );
	const uint8_t* rhsBytes = reinterpret_cast< const uint8_t* >( &rhs );
	for( int i = 0; i < sizeBytes; ++i ) {
		isEqual = isEqual && ( lhsBytes[ i ] == rhsBytes[ i ] );
	}
	return isEqual;
}


inline bool operator<( const drawSurf_t& surf0, const drawSurf_t& surf1 )
{
	if ( surf0.sortKey.materialId == surf1.sortKey.materialId ) {
		return ( surf0.objectId < surf1.objectId );
	} else {
		return ( surf0.sortKey.materialId < surf1.sortKey.materialId );
	}
}


template<> struct std::hash<drawSurf_t> {
	size_t operator()( drawSurf_t const& surf ) const {
		return Hash( reinterpret_cast<const uint8_t*>( &surf ), sizeof( surf ) );
	}
};


static inline bool SkipPass( const drawSurf_t& surf, const drawPass_t pass )
{
	if ( surf.pipelineObject[ pass ] == INVALID_HDL ) {
		return true;
	}

	if ( ( surf.flags & SKIP_OPAQUE ) != 0 )
	{
		if ( ( pass == DRAWPASS_SHADOW ) ||
			( pass == DRAWPASS_DEPTH ) ||
			( pass == DRAWPASS_TERRAIN ) ||
			( pass == DRAWPASS_OPAQUE ) ||
			( pass == DRAWPASS_SKYBOX ) ||
			( pass == DRAWPASS_DEBUG_SOLID )
			) {
			return true;
		}
	}

	if ( ( pass == DRAWPASS_DEBUG_SOLID ) && ( ( surf.flags & DEBUG_SOLID ) == 0 ) ) {
		return true;
	}

	if ( ( pass == DRAWPASS_DEBUG_WIREFRAME ) && ( ( surf.flags & WIREFRAME ) == 0 ) ) {
		return true;
	}

	return false;
}


enum resourceLifetime_t
{
	LIFETIME_TEMP,
	LIFETIME_PERSISTENT,
};


#if defined( USE_IMGUI )
struct imguiControls_t
{
	float		heightMapHeight;
	float		roughness;
	float		shadowStrength;
	float		toneMapColor[ 4 ];
	int			dbgImageId;
	int			selectedEntityId;
	bool		rebuildShaders;
	bool		raytraceScene;
	bool		rasterizeScene;
	bool		rebuildRaytraceScene;
	bool		openModelImportFileDialog;
	bool		openSceneFileDialog;
	bool		reloadScene;
	vec3f		selectedModelOrigin;
};
#endif