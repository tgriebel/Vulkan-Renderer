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
#include <atomic>

#define USE_IMGUI

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
const uint64_t	MaxSharedMemory					= 256 * MB;
const uint64_t	MaxLocalMemory					= 256 * MB;
const uint64_t	MaxFrameBufferMemory			= 128 * MB;

const uint32_t DEFAULT_DISPLAY_WIDTH			= 1280;
const uint32_t DEFAULT_DISPLAY_HEIGHT			= 720;

const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_FRAMES_STATES = ( MAX_FRAMES_IN_FLIGHT + 1 );

const std::string ModelPath = "models/";
const std::string TexturePath = "textures/";

using pipelineHdl_t = uint32_t;
const uint32_t INVALID_HANDLE = ~0;

class Renderer;

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

struct GpuProgram
{
	std::string				vsName;
	std::string				psName;
	std::vector<char>		vsBlob;
	std::vector<char>		psBlob;
	VkShaderModule			vs;
	VkShaderModule			ps;
	VkDescriptorSetLayout	descSetLayout;
	pipelineHdl_t			pipeline;
};

class refCount_t
{
public:
	refCount_t() = delete;

	refCount_t( const int count ) {
		assert( count > 0 );
		this->count = count;
	}
	inline int Add() {
		return ( count > 0 ) ? ++count : 0;
	}
	inline int Release() {
		return ( count > 0 ) ? --count : 0;
	}
	[[nodiscard]]
	inline int IsFree() const {
		return ( count <= 0 );
	}
private:
	int count; // Considered dead at 0
};


class hdl_t
{
public:
	hdl_t()
	{
		this->value = nullptr;
		this->instances = nullptr;
	};

	hdl_t( const int handle )
	{
		this->value = new int( handle );
		this->instances = new refCount_t( 1 );
	}

	hdl_t( const hdl_t& handle )
	{
		if ( handle.IsValid() )
		{
			this->value = handle.value;
			this->instances = handle.instances;
			this->instances->Add();
		} else {
			this->value = nullptr;
			this->instances = nullptr;
		}
	}

	~hdl_t()
	{
		if( IsValid() )
		{
			instances->Release();
			if ( instances->IsFree() )
			{
				delete instances;
				delete value;
			}
			instances = nullptr;
			value = nullptr;
		}
	}

	hdl_t& operator=( const hdl_t& handle )
	{
		if ( this != &handle )
		{
			this->~hdl_t();
			this->value = handle.value;
			this->instances = handle.instances;
			this->instances->Add();
		}
		return *this;
	}

	void Reset()
	{
		this->~hdl_t();
	}

	bool IsValid() const {
		return ( value != nullptr ) && ( instances != nullptr );
	}

	int Get() const {
		return ( IsValid() && ( instances->IsFree() == false ) ) ? *value : -1;
	}

	void Reassign( const int handle ) {
		if ( IsValid() ) {
			*value = handle;
		}
	}
private:
	int*		value;
	refCount_t*	instances;
};

struct pipelineObject_t;

struct material_t
{
	uint32_t				id; // TODO: remove?
	uint32_t				texture0;
	uint32_t				texture1;
	uint32_t				texture2;
	uint32_t				texture3;
	uint32_t				texture4;
	uint32_t				texture5;
	uint32_t				texture6;
	uint32_t				texture7;
	GpuProgram*				shaders[ DRAWPASS_COUNT ]; // TODO: use handle

	material_t()
	{
		id = 0;
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

template< class ResourceType > class Allocator;

struct allocRecord_t
{
	uint64_t	offset;
	uint64_t	size;
	uint64_t	alignment;
	bool		isValid;
};

template< class AllocatorType >
struct alloc_t
{
public:
	alloc_t() {
		allocator = nullptr;
	}

	uint64_t GetOffset() const {
		if( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return record->offset;
			}
		}
		return 0;
	}

	uint64_t GetSize() const {
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return record->size;
			}
		}
		return 0;
	}

	uint64_t GetAlignment() const {
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return record->alignment;
			}
		}
		return 0;
	}

	void* GetPtr() {
		if ( IsValid() )
		{
			const allocRecord_t* record = allocator->GetRecord( handle );
			if ( record != nullptr ) {
				return allocator->GetMemoryMapPtr( *record );
			}
		}
		return nullptr;
	}

	void Free() {
		if ( IsValid() ) {
			allocator->Free( handle );
		}
	}

private:
	bool IsValid() const {
		return ( allocator != nullptr ) && ( handle.Get() >= 0 );
	}

	hdl_t			handle;
	AllocatorType*	allocator;

	friend AllocatorType;
};

using AllocatorVkMemory = Allocator< VkDeviceMemory >;
using allocVk_t = alloc_t< AllocatorVkMemory >;

struct GpuImage
{
	VkImage			vk_image;
	VkImageView		vk_view;
	allocVk_t		allocation;
};

struct texture_t
{
	uint8_t*		bytes;
	uint32_t		sizeBytes;
	uint32_t		width;
	uint32_t		height;
	uint32_t		channels;
	uint32_t		mipLevels;
	bool			uploaded;

	GpuImage		image;
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


struct entity_t
{
	glm::mat4					matrix;
	uint32_t					materialId;
	uint32_t					modelId;
	renderFlags					flags;
};


class RenderView
{
public:
	RenderView()
	{
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)DEFAULT_DISPLAY_WIDTH;
		viewport.height = (float)DEFAULT_DISPLAY_HEIGHT;
		viewport.near = 1.0f;
		viewport.far = 0.0f;

		committedModelCnt = 0;
	}

	glm::mat4					viewMatrix;
	glm::mat4					projMatrix;
	viewport_t					viewport;
	light_t						lights[ MaxLights ];

	uint32_t					committedModelCnt;
	drawSurf_t					surfaces[ MaxModels ];
};


struct modelSource_t
{
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
};

struct DrawPassState
{
	VkRenderPass		pass;
	VkDescriptorSet		descriptorSets[ MAX_FRAMES_STATES ];
	VkFramebuffer		fb[ MAX_FRAMES_STATES ];
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
	mouse_t() : speed( 0.2f ),
				x( 0.0f ),
				y( 0.0f ),
				xPrev( 0.0f ),
				yPrev( 0.0f ),
				dx( 0.0f ),
				dy( 0.0f ),
				leftDown( false ),
				rightDown( false ),
				centered( false ) {}

	float	x;
	float	y;
	float	xPrev;
	float	yPrev;
	float	dx; // dx/dt
	float	dy; // dy/dt
	float	speed;
	bool	leftDown;
	bool	rightDown;
	bool	centered;
};


class Input
{
public:
	Input()
	{
		rBufferId = 0;
		wBufferId = 0;
		ClearKeyHistory();
	}

	bool IsKeyPressed( const char key )	{
		return keys[ rBufferId ][ key ];
	}

	const mouse_t& GetMouse() const {
		return mouse[ rBufferId ];
	}

	void NewFrame()
	{
		rBufferId = wBufferId;
		wBufferId = ( wBufferId + 1 ) % 2;
		mouse[ wBufferId ] = mouse[ rBufferId ];
		mouse[ wBufferId ].dx = 0.0f;
		mouse[ wBufferId ].dy = 0.0f;
		memcpy( keys[ wBufferId ], keys[ rBufferId ], 255 );
	}

private:
	mouse_t	mouse[ 2 ];
	int		rBufferId;
	int		wBufferId;
	bool	keys[ 2 ][ 256 ];

	void SetKey( const char key, const bool value ) {
		keys[ wBufferId ][ key ] = value;
	}

	mouse_t& GetMouseRef() {
		return mouse[ wBufferId ];
	}

	void ClearKeyHistory() {
		memset( keys[ 0 ], 0, 255 );
		memset( keys[ 1 ], 0, 255 );
	}

	friend void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
	friend void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
	friend void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
};

#if defined( USE_IMGUI )
struct imguiControls_t
{
	float		heightMapHeight;
	float		toneMapColor[ 4 ];
	int			dbgImageId;
	int			selectedModelId;
	glm::vec3	selectedModelOrigin;
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