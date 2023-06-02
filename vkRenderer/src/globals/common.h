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

#include <acceleration/aabb.h>

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

#include <math/vector.h>
#include <core/handle.h>
#include <image/color.h>
#include <resource_types/material.h>
#include <primitives/geom.h>
#include <scene/camera.h>
#include <resource_types/gpuProgram.h>
#include <syscore/common.h>
#include <sysCore/array.h>

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1000;
const uint32_t	DescriptorPoolMaxSamplers		= 3;
const uint32_t	DescriptorPoolMaxImages			= 1000;
const uint32_t	DescriptorPoolMaxComboImages	= 1000;
const uint32_t	DescriptorPoolMaxSets			= 128;
const uint32_t	MaxImageDescriptors				= 100;
const uint32_t	MaxPostImageDescriptors			= 3;
const uint32_t	MaxUboDescriptors				= 4;
const uint32_t	MaxLights						= 128;
const uint32_t	MaxParticles					= 1024;
const uint32_t	MaxViews						= 6;
const uint32_t	MaxModels						= 1000;
const uint32_t	ShadowObjectOffset				= MaxModels;
const uint32_t	MaxVertices						= 0x000FFFFF;
const uint32_t	MaxIndices						= 0x000FFFFF;
const uint32_t	MaxSurfaces						= MaxModels;
const uint32_t	MaxSurfacesDescriptors			= 1;
const uint32_t	MaxMaterials					= 256;
const uint32_t	MaxCodeImages					= 2;
const uint64_t	MaxSharedMemory					= MB( 1024 );
const uint64_t	MaxLocalMemory					= MB( 1024 );
const uint64_t	MaxFrameBufferMemory			= GB( 2 );

const uint32_t DEFAULT_DISPLAY_WIDTH			= 1280;
const uint32_t DEFAULT_DISPLAY_HEIGHT			= 720;

const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_FRAMES_STATES = ( MAX_FRAMES_IN_FLIGHT + 1 );

const std::string ModelPath = ".\\models\\";
const std::string TexturePath = ".\\textures\\";
const std::string BakePath = ".\\baked\\";
const std::string BakedModelExtension = ".mdl";

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


struct uniformBufferObject_t
{
	mat4x4f model;
};


struct viewBufferObject_t
{
	mat4x4f view;
	mat4x4f proj;
};


struct globalUboConstants_t
{
	vec4f		time;
	vec4f		generic;
	vec4f		dimensions;
	vec4f		shadowParms;
	vec4f		tonemap;
	uint32_t	numSamples;
	uint32_t	numLights;
	// uint32_t	pad[ 3 ]; // minUniformBufferOffsetAlignment
};

struct materialBufferObject_t
{
	int						textures[ Material::MaxMaterialTextures ];
	vec3f					Ka;
	float					Tr;
	vec3f					Ke;
	float					Ns;
	vec3f					Kd;
	float					Ni;
	vec3f					Ks;
	float					illum;
	vec3f					Tf;
	uint32_t				textured;
	uint32_t				pad[ 4 ]; // Multiple of minUniformBufferOffsetAlignment (0x40)
};


struct lightBufferObject_t
{
	vec4f		lightPos;
	vec4f		intensity;
	vec4f		lightDir;
	uint32_t	shadowViewId;
	uint32_t	pad[3];
};


struct particleBufferObject_t
{
	vec2f	position;
	vec2f	velocity;
	vec4f	color;
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR		capabilities;
	std::vector<VkSurfaceFormatKHR>	formats;
	std::vector<VkPresentModeKHR>	presentModes;
};


union sortKey_t
{
	uint32_t	materialId : 32;
	uint32_t	key;
};


struct drawSurf_t
{
	uint32_t			vertexOffset;
	uint32_t			vertexCount;
	uint32_t			firstIndex;
	uint32_t			indicesCnt;
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

class FrameBuffer;


enum normalDirection_t
{
	NORMAL_X_POS = 0,
	NORMAL_X_NEG = 1,
	NORMAL_Y_POS = 2,
	NORMAL_Y_NEG = 3,
	NORMAL_Z_POS = 4,
	NORMAL_Z_NEG = 5,
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

enum pipelineQueue_t {
	QUEUE_GRAPHICS,
	QUEUE_PRESENT,
	QUEUE_COMPUTE,
	QUEUE_COUNT,
};

template<class T>
class optional {

	std::pair<T, bool> option;

public:

	optional()
	{
		option.second = false;
	}

	bool has_value()
	{
		return option.second;
	}

	void set_value( const T& value_ )
	{
		option.first = value_;
		option.second = true;
	}

	T value()
	{
		return option.first;
	}
};


struct QueueFamilyIndices
{
	optional<uint32_t> graphicsFamily;
	optional<uint32_t> presentFamily;
	optional<uint32_t> computeFamily;

	bool IsComplete() {
		return	graphicsFamily.has_value() && 
				presentFamily.has_value() &&
				computeFamily.has_value();
	}
};

struct graphicsQueue_t
{
	VkQueue						graphicsQueue;
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffers[ MAX_FRAMES_STATES ];
	VkSemaphore					imageAvailableSemaphores[ MAX_FRAMES_STATES ];
	VkSemaphore					renderFinishedSemaphores[ MAX_FRAMES_STATES ];
	VkFence						inFlightFences[ MAX_FRAMES_STATES ];
	VkFence						imagesInFlight[ MAX_FRAMES_STATES ];
};

struct computeQueue_t
{
	VkQueue						queue;
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffers[ MAX_FRAMES_STATES ];
	VkSemaphore					semaphores[ MAX_FRAMES_STATES ];
};