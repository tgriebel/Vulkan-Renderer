#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <aabb.h>

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
#include <mathVector.h>

#include <color.h>

#define USE_IMGUI

const uint32_t KB_1 = 1024;
const uint32_t MB_1 = 1024 * KB_1;
const uint32_t GB_1 = 1024 * MB_1;

#define KB( N ) ( N * KB_1 )
#define MB( N ) ( N * MB_1 )
#define GB( N ) ( N * GB_1 )

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1000;
const uint32_t	DescriptorPoolMaxSamplers		= 3;
const uint32_t	DescriptorPoolMaxImages			= 1000;
const uint32_t	DescriptorPoolMaxComboImages	= 1000;
const uint32_t	DescriptorPoolMaxSets			= 8000;
const uint32_t	MaxImageDescriptors				= 100;
const uint32_t	MaxUboDescriptors				= 4;
const uint32_t	MaxViews						= 2;
const uint32_t	MaxModels						= 1000;
const uint32_t	ShadowObjectOffset				= MaxModels;
const uint32_t	MaxVertices						= 0x000FFFFF;
const uint32_t	MaxIndices						= 0x000FFFFF;
const uint32_t	MaxSurfaces						= MaxModels * MaxViews;
const uint32_t	MaxSurfacesDescriptors			= MaxModels * MaxViews;
const uint32_t	MaxMaterialDescriptors			= 64;
const uint32_t	MaxLights						= 3;
const uint32_t	MaxCodeImages					= 2;
const uint64_t	MaxSharedMemory					= MB( 1024 );
const uint64_t	MaxLocalMemory					= MB( 1024 );
const uint64_t	MaxFrameBufferMemory			= MB( 512 );

const uint32_t DEFAULT_DISPLAY_WIDTH			= 1280;
const uint32_t DEFAULT_DISPLAY_HEIGHT			= 720;

const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_FRAMES_STATES = ( MAX_FRAMES_IN_FLIGHT + 1 );

const std::string ModelPath = "models/";
const std::string TexturePath = "textures/";

using pipelineHdl_t = uint32_t;
const uint32_t INVALID_HANDLE = ~0;

uint32_t Hash( const uint8_t* bytes, const uint32_t sizeBytes );

class Renderer;

struct VertexInput
{
	vec3f pos;
	vec4f color;
	vec3f normal;
	vec3f tangent;
	vec3f bitangent;
	vec4f texCoord;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{ };
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof( VertexInput );
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static const uint32_t attribMax = 6;
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
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, normal );
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
		attributeDescriptions[ attribId ].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[ attribId ].offset = offsetof( VertexInput, texCoord );
		++attribId;

		assert( attribId == attribMax );

		return attributeDescriptions;
	}

	bool operator==( const VertexInput& other ) const
	{
		return	( pos == other.pos ) && 
				( color == other.color ) &&
				( normal == other.normal ) &&
				( tangent == other.tangent ) &&
				( bitangent == other.bitangent ) &&
				( texCoord == other.texCoord );
	}
};


enum drawPass_t : uint32_t
{
	DRAWPASS_SHADOW,
	DRAWPASS_DEPTH,
	DRAWPASS_TERRAIN,
	DRAWPASS_OPAQUE,
	DRAWPASS_SKYBOX,
	DRAWPASS_TRANS,
	DRAWPASS_DEBUG_SOLID,
	DRAWPASS_DEBUG_WIREFRAME,
	DRAWPASS_POST_2D,
	DRAWPASS_COUNT
};

enum renderFlags_t : uint32_t
{
	NONE		= 0,
	HIDDEN		= ( 1 << 0 ),
	NO_SHADOWS	= ( 1 << 1 ),
	WIREFRAME	= ( 1 << 2 ),	// FIXME: this needs to make a unique pipeline object if checked
	DEBUG_SOLID	= ( 1 << 3 ),	// "
	SKIP_OPAQUE	= ( 1 << 4 ),
	COMMITTED	= ( 1 << 5 ),
};

enum shaderType_t : uint32_t
{
	UNSPECIFIED = 0,
	VERTEX,
	PIXEL,
	COMPUTE,
};

struct shaderSource_t
{
	std::string			name;
	std::vector<char>	blob;
	shaderType_t		type;
};

struct GpuProgram
{
	static const uint32_t MaxShaders = 2;

	shaderSource_t			shaders[ MaxShaders ];
	VkShaderModule			vk_shaders[ MaxShaders ];
	pipelineHdl_t			pipeline;
	uint32_t				shaderCount;
	bool					isCompute;
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
			if( handle.IsValid() ) {
				this->instances->Add();
			}
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
#define INVALID_HDL hdl_t()

struct pipelineObject_t;

struct Material
{
	static const uint32_t MaxMaterialTextures = 8;

	hdl_t					textures[ MaxMaterialTextures ];
	hdl_t					shaders[ DRAWPASS_COUNT ];

	rgbTuplef_t				Ka;
	rgbTuplef_t				Ke;
	rgbTuplef_t				Kd;
	rgbTuplef_t				Ks;
	rgbTuplef_t				Tf;
	float					Tr;
	float					Ns;
	float					Ni;
	float					d;
	float					illum;

	bool					textured;

	Material() :
		Tr( 0.0f ),
		Ns( 0.0f ),
		Ni( 0.0f ),
		d( 1.0f ),
		illum( 0.0f )
	{
		for ( int i = 0; i < MaxMaterialTextures; ++i ) {
			textures[ i ] = 0;
		}
		for ( int i = 0; i < DRAWPASS_COUNT; ++i ) {
			shaders[ i ] = INVALID_HDL;
		}
	}
};


template<> struct std::hash<VertexInput>
{
	size_t operator()( VertexInput const& vertex ) const
	{
		return Hash( reinterpret_cast< const uint8_t* >( &vertex ), sizeof( VertexInput ) );
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
	mat4x4f model;
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
	// uint32_t	pad[ 3 ]; // minUniformBufferOffsetAlignment
};

struct materialBufferObject_t
{
	int						textures[ Material::MaxMaterialTextures ];
	vec4f					Ka;
	vec4f					Ke;
	vec4f					Kd;
	vec4f					Ks;
	vec4f					Tf;
	float					Tr;
	float					Ns;
	float					Ni;
	float					d;
	float					illum;
	uint32_t				pad[ 15 ]; // Multiple of minUniformBufferOffsetAlignment (0x40)
};

struct light_t
{
	vec4f		lightPos;
	vec4f		intensity;
	vec4f		lightDir;
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

enum textureType_t
{
	TEXTURE_TYPE_UNKNOWN,
	TEXTURE_TYPE_2D,
	TEXTURE_TYPE_CUBE,
};

struct textureInfo_t {
	uint32_t		width;
	uint32_t		height;
	uint32_t		channels;
	uint32_t		mipLevels;
	uint32_t		layers;
	textureType_t	type;
};

struct texture_t
{
	uint8_t*		bytes;
	uint32_t		sizeBytes;
	textureInfo_t	info;
	bool			uploaded;

	GpuImage		image;

	texture_t() {
		info.width = 0;
		info.height = 0;
		info.channels = 0;
		info.mipLevels = 0;
		info.type = TEXTURE_TYPE_UNKNOWN;
		uploaded = false;
		bytes = nullptr;
	}
};


struct drawSurf_t
{
	uint32_t			vertexOffset;
	uint32_t			vertexCount;
	uint32_t			firstIndex;
	uint32_t			indicesCnt;
	uint32_t			objectId;
	uint32_t			materialId;
	renderFlags_t		flags;
	uint8_t				stencilBit;

	pipelineHdl_t		pipelineObject[ DRAWPASS_COUNT ];

	inline bool operator()( const drawSurf_t& surf ) {
		return Hash( reinterpret_cast<const uint8_t*>( &surf ), sizeof( surf ) );
	}
};


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
	if ( surf0.materialId == surf1.materialId ) {
		return ( surf0.objectId < surf1.objectId );
	} else {
		return ( surf0.materialId < surf1.materialId );
	}
}

template<> struct std::hash<drawSurf_t> {
	size_t operator()( drawSurf_t const& surf ) const {
		return Hash( reinterpret_cast<const uint8_t*>( &surf ), sizeof( surf ) );
	}
};

struct surface_t {
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


struct modelSource_t
{
	static const uint32_t		MaxSurfaces = 10;
	AABB						bounds;
	surface_t					surfs[ MaxSurfaces ];
	surfUpload_t				upload[ MaxSurfaces ];
	uint32_t					surfCount;
};

enum entityFlags_t {
	ENT_FLAG_NONE,
	ENT_FLAG_SELECTABLE,
};

class Entity
{
public:
	Entity() {
		matrix = mat4x4f( 1.0f );
		modelId = -1;
		materialId = -1;
		flags = ENT_FLAG_NONE;
		renderFlags = renderFlags_t::NONE;
		outline = false;
	}

	bool			outline;
	int				modelId;
	int				materialId;

	AABB			GetBounds() const;
	vec3f			GetOrigin() const;
	void			SetOrigin( const vec3f& origin );
	void			SetScale( const vec3f& scale );
	void			SetRotation( const vec3f& xyzDegrees );
	mat4x4f			GetMatrix() const;
	void			SetFlag( const entityFlags_t flag );
	void			ClearFlag( const entityFlags_t flag );
	bool			HasFlag( const entityFlags_t flag ) const;
	void			SetRenderFlag( const renderFlags_t flag );
	void			ClearRenderFlag( const renderFlags_t flag );
	bool			HasRenderFlag( const renderFlags_t flag ) const;
	renderFlags_t	GetRenderFlags() const;

private:
	renderFlags_t	renderFlags;
	entityFlags_t	flags;
	mat4x4f			matrix;
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
		mergedModelCnt = 0;
	}

	mat4x4f										viewMatrix;
	mat4x4f										projMatrix;
	viewport_t									viewport;
	light_t										lights[ MaxLights ];

	uint32_t									committedModelCnt;
	uint32_t									mergedModelCnt;
	drawSurf_t									surfaces[ MaxModels ];
	drawSurf_t									merged[ MaxModels ];
	drawSurfInstance_t							instances[ MaxModels ];
	uint32_t									instanceCounts[ MaxModels ];
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
	float		roughness;
	float		shadowStrength;
	float		toneMapColor[ 4 ];
	int			dbgImageId;
	int			selectedModelId;
	bool		rebuildShaders;
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
	VkCommandBuffer				commandBuffer;
	VkSemaphore					semaphore;
};

static inline void RandSphere( float& theta, float& phi )
{
	const float u = ( (float)rand() / ( RAND_MAX ) );
	const float v = ( (float)rand() / ( RAND_MAX ) );
	theta = 2.0f * 3.14159f * u;
	phi = acos( 2.0f * v - 1.0f );
}

static inline void RandSpherePoint( const float radius, vec3f& outPoint )
{
	float theta;
	float phi;
	RandSphere( theta, phi );

	outPoint[ 0 ] = radius * sin( phi ) * cos( theta );
	outPoint[ 1 ] = radius * sin( phi ) * sin( theta );
	outPoint[ 2 ] = radius * cos( phi );
}

static inline void RandPlanePoint( vec2f& outPoint )
{
	outPoint[ 0 ] = ( (float) rand() / ( RAND_MAX ) );
	outPoint[ 1 ] = ( (float) rand() / ( RAND_MAX ) );
}

// Fowler?Noll?Vo_hash_function - fnv1a - 32bits
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static inline uint32_t Hash( const uint8_t* bytes, const uint32_t sizeBytes ) {
	uint32_t result = 2166136261;
	const uint32_t prime = 16777619;
	for ( uint32_t i = 0; i < sizeBytes; ++i ) {
		result = ( result ^ bytes[ i ] ) * prime;
	}
	return result;
}

static inline uint64_t Hash( const std::string& s ) {
	const int p = 31;
	const int m = static_cast<int>( 1e9 + 9 );
	uint64_t hash = 0;
	uint64_t pN = 1;
	const int stringLen = static_cast<int>( s.size() );
	for ( int i = 0; i < stringLen; ++i )
	{
		hash = ( hash + ( s[ i ] - (uint64_t)'a' + 1ull ) * pN ) % m;
		pN = ( pN * p ) % m;
	}
	return hash;
}

static inline vec4f glmToVec4( const glm::vec4& glmVec ) {
	return vec4f( glmVec.x, glmVec.y, glmVec.z, glmVec.w );
}

static inline glm::vec4 vecToGlm4( const vec4f& vec ) {
	return glm::vec4( vec[ 0 ], vec[ 1 ], vec[ 2 ], vec[ 3 ] );
}

static inline vec3f glmToVec3( const glm::vec3& glmVec ) {
	return vec3f( glmVec.x, glmVec.y, glmVec.z );
}

static inline glm::vec3 vecToGlm3( const vec3f& vec ) {
	return glm::vec3( vec[ 0 ], vec[ 1 ], vec[ 2 ] );
}