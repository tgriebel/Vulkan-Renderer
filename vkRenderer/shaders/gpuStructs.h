#ifndef GPU_STRUCTS_HLSL_H
#define GPU_STRUCTS_HLSL_H

// This file is shared between C++ and HLSL.
// The languages are syntactically similar so basic things like structs can be shared with some finesse
// It's a common problem keeping parallel structs in sync; this resolves that without needing a reflection system

// `SHADER_STRUCTS_CPP` and `SHADER_STRUCTS_HLSL` can be used where language differ
// Alignment, minimum struct sizes, padding need to be considered--always. Stick to 16-byte alignment generally

#ifdef SHADER_STRUCTS_CPP
#include <cassert>
#define float4x4 mat4x4f
#define float3x3 mat3x3f
#define float2x2 mat2x2f
#define float4 vec4f
#define float3 vec3f
#define float2 vec2f
#define uint uint32_t
#define int int32_t

static_assert( sizeof( mat4x4f ) == 64 );
static_assert( sizeof( mat3x3f ) == 36 );
static_assert( sizeof( mat2x2f ) == 16 );
static_assert( sizeof( vec4f ) == 16 );
static_assert( sizeof( vec3f ) == 12 );
static_assert( sizeof( vec2f ) == 8 );

#define BIND_SLOT( x )
#define SEMANTIC( x )
#endif


#ifdef SHADER_STRUCTS_HLSL
#define BIND_SLOT( x )		[[vk::location( x )]]
#define BIND_SET( S, N )    [[vk::binding( N, S )]]
#define BIND_INLINE         [[vk::push_constant]]
#define SEMANTIC( x ) : x
#endif


#define MaxLights					128
#define MaxMaterials				256
#define MaxViews					15
#define MaxSurfaces					1000
#define MaxMaterialTextures			8
#define MaxMaterialExtraDataBytes	256

enum ggxTextureSlot_t
{
	GGX_ALBEDO_MAP_SLOT			= 0,
	GGX_NORMAL_MAP_SLOT			= 1,
	GGX_ROUGHNESS_MAP_SLOT		= 2,
	GGX_METALLIC_MAP_SLOT		= 3,
	GGX_CLEARCOAT_NML_MAP_SLOT	= 4,
};


enum blinnPhongTextureSlot_t
{
	BLINN_PHONG_COLOR_MAP_SLOT	= 0,
	BLINN_PHONG_NORMAL_MAP_SLOT	= 1,
	BLINN_PHONG_SPEC_MAP_SLOT	= 2,
};


enum cubeTextureSlot_t
{
	CUBE_FRONT_SLOT				= 0,
	CUBE_BACK_SLOT				= 1,
	CUBE_TOP_SLOT				= 2,
	CUBE_BOTTOM_SLOT			= 3,
	CUBE_RIGHT_SLOT				= 4,
	CUBE_LEFT_SLOT				= 5,
};


enum hgtTextureSlot_t
{
	HGT_HEIGHT_MAP_SLOT			= 0,
	HGT_COLOR_MAP_SLOT0			= 1,
	HGT_COLOR_MAP_SLOT1			= 2,
};


struct gpuGlobals_t
{
	float4  time;
	float4  generic;
	float4  shadowParms;
	float4  toneMapTint;
	float4  bloom;
	float4  exposure;
	float4  exposure2;
	float4  dof;
	float4  chromaticAberration;
	uint    numSamples;
	uint    whiteId;
	uint    blackId;
	uint    defaultAlbedoId;
	uint    defaultNormalId;
	uint    defaultRoughnessId;
	uint    defaultMetalId;
	uint    defaultImageId;
	uint    brdfLutId;
	uint    isTextured;
	uint    shadow2dCount;
	uint    shadowCubeCount;
	uint    textureCount;
	uint    materialCount;
	uint    useDiffuseIBL;
	uint    useSpecularIBL;
};


struct gpuMaterial_t
{
	int     textureId[ MaxMaterialTextures ];
	float3  Ka;
	float   Tr;
	float3  Ke;
	float   Ns;
	float3  Kd;
	float   Ni;
	float3  Ks;
	float   illum;
	float3  Tf;
	float	roughness;
	float	metalness;
	float	sheen;
	float	clearcoatThickness;
	float	clearcoatRoughness;
	float	anisotropy;
	float	anisotropyRotation;
	uint    textured;
	uint    pad0;
	uint    extraData[ MaxMaterialExtraDataBytes ];
};


struct gpuLight_t
{
	float4 lightPos;
	float4 intensity;
	float4 lightDir;
	uint   shadowViewId;
	uint   pad0;
	uint   pad1;
	uint   pad2;
};


struct gpuView_t
{
	float4x4 viewMat;
	float4x4 projMat;
	float4   dimensions;
	float3   viewOrigin;
	uint     numLights;
};


struct gpuPass_t
{
	uint codeImageCount;
};


struct gpuSurface_t
{
	float4x4 model;
	uint     diffuseIblCubeId;
	uint     envCubeId;
	uint     pad[ 14 ];
};


struct gpuParticle_t
{
	float2	position;
	float2	velocity;
	float4	color;
};


struct gpuPushConstants_t
{
	uint objectId;
	uint materialId;
	uint viewId;
};


struct vsInput_t
{ 
	BIND_SLOT( 0 ) float3 inPosition	SEMANTIC( POSITION );
	BIND_SLOT( 1 ) float4 inColor		SEMANTIC( COLOR0 );
	BIND_SLOT( 2 ) float3 inNormal		SEMANTIC( NORMAL );
	BIND_SLOT( 3 ) float3 inTangent		SEMANTIC( TANGENT );
	BIND_SLOT( 4 ) float3 inBitangent	SEMANTIC( BINORMAL );
	BIND_SLOT( 5 ) float4 inTexCoord	SEMANTIC( TEXCOORD0 );
};


#ifdef SHADER_STRUCTS_CPP
#undef float4x4
#undef float3x3
#undef float2x2
#undef float4
#undef float3
#undef float2
#undef uint
#undef int
#endif

#endif
