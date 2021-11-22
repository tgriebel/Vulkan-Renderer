#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

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

const uint32_t KB = 1024;
const uint32_t MB = 1024 * KB;
const uint32_t GB = 1024 * MB;

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1000;
const uint32_t	DescriptorPoolMaxSamplers		= 3;
const uint32_t	DescriptorPoolMaxImages			= 1000;
const uint32_t	DescriptorPoolMaxComboImages	= 1000;
const uint32_t	DescriptorPoolMaxSets			= 8000;
const uint32_t	MaxImageDescriptors				= 100;
const uint32_t	MaxUboDescriptors				= 4;
const uint32_t	MaxViews						= 2;
const uint32_t	MaxModels						= 1000;
const uint32_t	MaxVertices						= 0x000FFFFF;
const uint32_t	MaxIndices						= 0x000FFFFF;
const uint32_t	MaxSurfaces						= MaxModels * MaxViews;
const uint32_t	MaxSurfacesDescriptors			= MaxModels * MaxViews;
const uint32_t	MaxMaterialDescriptors			= 12;
const uint32_t	MaxLights						= 3;
const uint32_t	MaxCodeImages					= 1;
const uint64_t	MaxSharedMemory					= 512 * MB;
const uint64_t	MaxLocalMemory					= 512 * MB;

const uint32_t DISPLAY_WIDTH					= 1280;
const uint32_t DISPLAY_HEIGHT					= 720;

const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_FRAMES_STATES = ( MAX_FRAMES_IN_FLIGHT + 1 );

const std::string ModelPath = "models/";
const std::string TexturePath = "textures/";

using pipelineHdl_t = uint32_t;
const uint32_t INVALID_HANDLE = ~0;

class VkRenderer;

struct VertexInput
{
	glm::vec3 pos;
	glm::vec4 color;
	//glm::vec3 normal;
	glm::vec3 bitangent;
	glm::vec3 tangent;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{ };
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof( VertexInput );
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static const uint32_t attribMax = 5;
	static std::array<VkVertexInputAttributeDescription, attribMax> GetAttributeDescriptions()
	{
		uint32_t attribId = 0;

		std::array<VkVertexInputAttributeDescription, attribMax> attributeDescriptions{ };
		attributeDescriptions[ attribId ].binding = 0;
		attributeDescriptions[ attribId ].location = attribId;
		attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, pos );
		++attribId;

		attributeDescriptions[ attribId ].binding = 0;
		attributeDescriptions[ attribId ].location = attribId;
		attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, color );
		++attribId;

		attributeDescriptions[ attribId ].binding = 0;
		attributeDescriptions[ attribId ].location = attribId;
		attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, tangent );
		++attribId;

		attributeDescriptions[ attribId ].binding = 0;
		attributeDescriptions[ attribId ].location = attribId;
		attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, bitangent );      
		++attribId;

		attributeDescriptions[ attribId ].binding = 0;
		attributeDescriptions[ attribId ].location = attribId;
		attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, texCoord );
		++attribId;

		assert( attribId == attribMax );

		return attributeDescriptions;
	}

	bool operator==( const VertexInput& other ) const
	{
		return pos == other.pos && color == other.color && tangent == other.tangent && texCoord == other.texCoord;
	}
};


enum drawPass_t : uint32_t
{
	DRAWPASS_SHADOW,
	DRAWPASS_DEPTH,
	DRAWPASS_TERRAIN,
	DRAWPASS_OPAQUE,
	DRAWPASS_TRANS,
	DRAWPASS_SKYBOX,
	DRAWPASS_WIREFRAME,
	DRAWPASS_POST_2D,
	DRAWPASS_COUNT
};

enum drawPassFlags_t : uint32_t
{
	DRAWPASS_FLAG_SHADOW	= ( 1 << DRAWPASS_SHADOW ),
	DRAWPASS_FLAG_DEPTH		= ( 1 << DRAWPASS_DEPTH ),
	DRAWPASS_FLAG_TERRAIN	= ( 1 << DRAWPASS_TERRAIN ),
	DRAWPASS_FLAG_OPAQUE	= ( 1 << DRAWPASS_OPAQUE ),
	DRAWPASS_FLAG_TRANS		= ( 1 << DRAWPASS_TRANS ),
	DRAWPASS_FLAG_SKYBOX	= ( 1 << DRAWPASS_SKYBOX ),
	DRAWPASS_FLAG_WIRE		= ( 1 << DRAWPASS_WIREFRAME ),
	DRAWPASS_FLAG_POST_2D	= ( 1 << DRAWPASS_POST_2D ),
};

enum renderFlags : uint32_t
{
	NONE		= 0,
	NO_SHADOWS	= ( 1 << 0 ),
	WIREFRAME	= ( 1 << 1 ),
};


struct RenderProgram
{
	VkShaderModule			vs;
	VkShaderModule			ps;
	VkDescriptorSetLayout	descSetLayout;
	pipelineHdl_t			pipeline;
};


struct pipelineObject_t;

struct material_t
{
	uint32_t				id;
	uint32_t				texture0;
	uint32_t				texture1;
	uint32_t				texture2;
	uint32_t				texture3;
	uint32_t				texture4;
	uint32_t				texture5;
	uint32_t				texture6;
	uint32_t				texture7;
	RenderProgram*			shaders[ DRAWPASS_COUNT ]; // TODO: use handle

	material_t()
	{
		texture0 = 0;
		texture1 = 0;
		texture2 = 0;
		texture3 = 0;
		texture4 = 0;
		texture5 = 0;
		texture6 = 0;
		texture7 = 0;
		for ( int i = 0; i < DRAWPASS_COUNT; ++i )
		{
			shaders[ i ] = nullptr;
		}
	}
};


template<> struct std::hash<VertexInput>
{
	size_t operator()( VertexInput const& vertex ) const
	{
		return ( ( hash<glm::vec3>()( vertex.pos ) ^ ( hash<glm::vec3>()( vertex.color ) << 1 ) ) >> 1 ) ^ ( hash<glm::vec2>()( vertex.texCoord ) << 1 ) ^ ( hash<glm::vec2>()( vertex.tangent ) << 1 );
	}
};


struct viewport_t
{
	float x;
	float y;
	float width;
	float height;
	float near;
	float far;
};


struct uniformBufferObject_t
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct globalUboConstants_t
{
	glm::vec4	time;
	glm::vec4	heightmap;
	glm::vec4	tonemap;
	uint32_t	pad[ 4 ]; // minUniformBufferOffsetAlignment
};

struct materialBufferObject_t
{
	uint32_t texture0;
	uint32_t texture1;
	uint32_t texture2;
	uint32_t texture3;
	uint32_t texture4;
	uint32_t texture5;
	uint32_t texture6;
	uint32_t texture7;
	uint32_t pad[ 8 ]; // minUniformBufferOffsetAlignment
};

struct light_t
{
	glm::vec4	lightPos;
	glm::vec4	intensity;
	glm::vec4	lightDir;
	uint32_t	pad[ 4 ];
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR		capabilities;
	std::vector<VkSurfaceFormatKHR>	formats;
	std::vector<VkPresentModeKHR>	presentModes;
};

class MemoryAllocator;

struct AllocRecord
{
	VkDeviceSize		offset;
	VkDeviceSize		subOffset;
	VkDeviceSize		size;
	VkDeviceSize		alignment;
	MemoryAllocator*	memory;
};


class MemoryAllocator
{
public:

	MemoryAllocator()
	{
		Unbind();
	}

	MemoryAllocator( VkDeviceMemory& _memory, const VkDeviceSize _size, const uint32_t _type )
	{
		offset = 0;
		memory = _memory;
		size = _size;
		type = _type;
	}

	void Bind( VkDeviceMemory& _memory, void* memMap, const VkDeviceSize _size, const uint32_t _type )
	{
		offset = 0;
		memory = _memory;
		size = _size;
		type = _type;
		ptr = memMap;
	}

	void Unbind()
	{
		offset = 0;
		memory = nullptr;
		size = 0;
		type = 0;
		ptr = nullptr;
	}

	VkDeviceMemory& GetDeviceMemory()
	{
		return memory;
	}

	bool IsMemoryCompatible( const uint32_t memoryType ) const
	{
		return ( type == memoryType );
	}

	void* GetMemoryMapPtr( AllocRecord& record ) {
		if ( ptr == nullptr ) {
			return nullptr;
		}
		if ( ( record.offset + record.size ) > size ) { // starts from 0 offset
			return nullptr;
		}

		return static_cast<void*>( (uint8_t*)ptr + record.offset );
	}

	VkDeviceSize GetOffset() const
	{
		return offset;
	}

	VkDeviceSize GetAlignedOffset( const VkDeviceSize alignment ) const
	{
		const VkDeviceSize boundary = ( offset % alignment );
		const VkDeviceSize nextOffset = ( boundary == 0 ) ? boundary : ( alignment - boundary );

		return ( offset + nextOffset );
	}

	bool CanAllocate( VkDeviceSize alignment, VkDeviceSize allocSize ) const
	{
		const VkDeviceSize nextOffset = GetAlignedOffset( alignment );

		return ( ( nextOffset + allocSize ) < size );
	}

	bool CreateAllocation( VkDeviceSize alignment, VkDeviceSize allocSize, AllocRecord& subAlloc )
	{
		if ( !CanAllocate( alignment, allocSize ) )
		{
			return false;
		}

		const VkDeviceSize nextOffset = GetAlignedOffset( alignment );

		subAlloc.memory			= this;
		subAlloc.offset			= nextOffset;
		subAlloc.subOffset		= 0;
		subAlloc.size			= allocSize;
		subAlloc.alignment		= alignment;

		offset = nextOffset + allocSize;

		return true;
	}

private:
	uint32_t		type;
	VkDeviceSize	size;
	VkDeviceSize	offset;
	VkDeviceMemory	memory;
	void*			ptr;
};


struct texture_t
{
	uint32_t		width;
	uint32_t		height;
	uint32_t		channels;
	uint32_t		mipLevels;

	AllocRecord		memory;

	VkImage			vk_image;	
	VkImageView		vk_imageView;
};


struct drawSurf_t
{
	glm::mat4			modelMatrix;

	uint32_t			vertexOffset;
	uint32_t			vertexCount;
	uint32_t			firstIndex;
	uint32_t			indicesCnt;
	uint32_t			objectId;
	uint32_t			materialId;
	renderFlags			flags;

	pipelineHdl_t		pipelineObject[ DRAWPASS_COUNT ];
};


struct RenderView
{
	glm::mat4					viewMatrix;
	glm::mat4					projMatrix;
	viewport_t					viewport;
	light_t						lights[ MaxLights ];

	uint32_t					committedModelCnt;
	drawSurf_t					surfaces[ MaxModels ];
};


struct modelSource_t
{
	std::string					name;
	material_t*					material;
	uint32_t					materialId;
	std::vector<VertexInput>	vertices;
	std::vector<uint32_t>		indices;
};


struct surfUpload_t
{
	uint32_t					vertexCount;
	uint32_t					indexCount;
	uint32_t					vertexOffset;
	uint32_t					firstIndex;
	VkBuffer					vb;
	VkBuffer					ib;
	// TODO: remove
	AllocRecord					vbMemory;
	AllocRecord					ibMemory;
};


struct entity_t
{
	glm::mat4					matrix;
	uint32_t					materialId;	
	uint32_t					modelId;
	renderFlags					flags;
};


struct DeviceBuffer
{
	VkBuffer			buffer;
	AllocRecord			allocation;
	VkDeviceSize		currentOffset;
	VkDeviceSize		size;

	void Reset()
	{
		currentOffset = 0;
	}

	VkBuffer GetVkObject()
	{
		return buffer;
	}

	void CopyData( void* data, const size_t sizeInBytes )
	{
		void* mappedData = allocation.memory->GetMemoryMapPtr( allocation );
		if ( mappedData != nullptr )
		{
			memcpy( (uint8_t*) mappedData + currentOffset, data, sizeInBytes );
			currentOffset += static_cast<uint32_t>( ( sizeInBytes + ( allocation.alignment - 1 ) ) & ~( allocation.alignment - 1 ) );
		}
	}
};


struct DeviceImage
{
	VkImage		image;
	VkImageView	view;
	AllocRecord	allocation;
};


struct FrameBufferState
{
	VkRenderPass	pass;
	VkFramebuffer	fb[ MAX_FRAMES_STATES ];
};


enum normalDirection_t
{
	NORMAL_X_POS = 0,
	NORMAL_X_NEG = 1,
	NORMAL_Y_POS = 2,
	NORMAL_Y_NEG = 3,
	NORMAL_Z_POS = 4,
	NORMAL_Z_NEG = 5,
};


struct mouse_t
{
	double x;
	double y;

	bool leftDown;
	bool rightDown;
};


struct input_t
{
	bool	keys[ 256 ];
	bool	altDown;
};

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

struct deviceContext_t
{
	VkDevice					device;
	VkPhysicalDevice			physicalDevice;
//	VkRenderPass				renderPass;
//	VkDescriptorSetLayout		descriptorSetLayout;
//	std::vector<RenderProgram>	programs;
//	MemoryAllocator				localMemory;
//	MemoryAllocator				sharedMemory;
};

static inline void RandSphere( float& theta, float& phi )
{
	const float u = ( (float)rand() / ( RAND_MAX ) );
	const float v = ( (float)rand() / ( RAND_MAX ) );
	theta = 2.0f * 3.14159f * u;
	phi = acos( 2.0f * v - 1.0f );
}

static inline void RandSpherePoint( const float radius, glm::vec3& outPoint )
{
	float theta;
	float phi;
	RandSphere( theta, phi );

	outPoint[ 0 ] = radius * sin( phi ) * cos( theta );
	outPoint[ 1 ] = radius * sin( phi ) * sin( theta );
	outPoint[ 2 ] = radius * cos( phi );
}

static inline void RandPlanePoint( glm::vec2& outPoint )
{
	outPoint.x = ( (float) rand() / ( RAND_MAX ) );
	outPoint.y = ( (float) rand() / ( RAND_MAX ) );
}

extern VkDevice					device;
extern VkDescriptorSetLayout	globalLayout;

VkImageView CreateImageView( VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels );