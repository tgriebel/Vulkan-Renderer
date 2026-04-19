#pragma once

#include <cinttypes>
#include <GfxCore/core/common.h>
#include <GfxCore/math/vector.h>
#include <GfxCore/math/matrix.h>
#include <GfxCore/image/color.h>
#include <SysCore/handle.h>
#include "asset.h"


class Serializer;

struct lightInput_t
{
	vec3f	viewVector;
	vec3f	normal;
};

enum drawPass_t : uint32_t
{
	DRAWPASS_SHADOW,
	DRAWPASS_DEPTH,
	DRAWPASS_TERRAIN,
	DRAWPASS_OPAQUE,
	DRAWPASS_SKYBOX,
	DRAWPASS_TRANS,
	DRAWPASS_EMISSIVE,
	DRAWPASS_DEBUG_3D,
	DRAWPASS_DEBUG_WIREFRAME,
	DRAWPASS_2D,
	DRAWPASS_DEBUG_2D,
	DRAWPASS_COUNT,

	DRAWPASS_SHADOW_BEGIN = DRAWPASS_SHADOW,
	DRAWPASS_SHADOW_END = DRAWPASS_SHADOW,
	DRAWPASS_MAIN_BEGIN = DRAWPASS_DEPTH,
	DRAWPASS_MAIN_END = DRAWPASS_DEBUG_WIREFRAME,
	DRAWPASS_2D_BEGIN = DRAWPASS_2D,
	DRAWPASS_2D_END = DRAWPASS_DEBUG_2D,
};

enum textureSlot_t : uint32_t
{
	TEXTURE_SLOT_0,
	TEXTURE_SLOT_1,
	TEXTURE_SLOT_2,
	TEXTURE_SLOT_3,
	TEXTURE_SLOT_4,
	TEXTURE_SLOT_5,
	TEXTURE_SLOT_6,
	TEXTURE_SLOT_7,
};

enum ggxTextureSlot_t : uint32_t
{
	GGX_ALBEDO_MAP_SLOT			= 0,
	GGX_NORMAL_MAP_SLOT			= 1,
	GGX_ROUGHNESS_MAP_SLOT		= 2,
	GGX_METALLIC_MAP_SLOT		= 3,
	GGX_CLEARCOAT_NML_MAP_SLOT	= 4,
};

enum blinnPhongTextureSlot_t : uint32_t
{
	GGX_COLOR_MAP_SLOT			= 0,
	GGX_NORMAL_MAP_SLOT			= 1,
	GGX_SPEC_MAP_SLOT			= 2,
};

enum cubeTextureSlot_t : uint32_t
{
	CUBE_FRONT_SLOT				= 0,
	CUBE_BACK_SLOT				= 1,
	CUBE_TOP_SLOT				= 2,
	CUBE_BOTTOM_SLOT			= 3,
	CUBE_RIGHT_SLOT				= 4,
	CUBE_LEFT_SLOT				= 5,
};

enum hgtTextureSlot_t : uint32_t
{
	HGT_HEIGHT_MAP_SLOT,
	HGT_COLOR_MAP_SLOT0,
	HGT_COLOR_MAP_SLOT1,
};

enum materialUsage_t : uint32_t
{
	MATERIAL_USAGE_UNKNOWN,
	MATERIAL_USAGE_CODE,
	MATERIAL_USAGE_GGX,
	MATERIAL_USAGE_HEIGHT_MAP,
	MATERIAL_USAGE_CUBE,
	MATERIAL_USAGE_BLINN_PHONG,
};


struct materialParms_t
{
	materialParms_t() : Tr( 0.0f ), Ns( 0.0f ), Ni( 0.0f ), d( 1.0f ), illum( 0.0f ), roughness( 0.0f ), metalness( 1.0f ) {}

	rgb32_t		Ka;
	rgb32_t		Ke;
	rgb32_t		Kd;
	rgb32_t		Ks;
	rgb32_t		Tf;
	float		Tr;
	float		Ns;
	float		Ni;
	float		d;
	float		illum;
	float		roughness;
	float		metalness;
	float		sheen;
	float		clearcoatThickness;
	float		clearcoatRoughness;
	float		anisotropy;
	float		anisotropyRotation;
};

class Material
{
public:
	static const uint32_t Version = 1;
	static const uint32_t MaxMaterialTextures = 8;
	static const uint32_t MaxExtraDataBytes = 256;
	static const uint32_t MaxMaterialShaders = DRAWPASS_COUNT;

	int32_t					uploadId;
	materialUsage_t			usage;

private:
	materialParms_t			p;
	uint32_t				extraData[ MaxExtraDataBytes ];
	uint32_t				extraByteCount;
	uint16_t				textureBitSet;
	uint16_t				shaderBitSet;

	hdl_t					textures[ MaxMaterialTextures ];
	hdl_t					shaders[ MaxMaterialShaders ];
	uint32_t				shaderPerms[ MaxMaterialShaders ];	// Per-pass permutation bitmask (maps to shaderPermId_t)

public:
	Material() :
		textureBitSet( 0 ),
		shaderBitSet( 0 )
	{
		p.Kd = rgb32_t( 1.0f, 1.0f, 1.0f );
		for ( int i = 0; i < MaxMaterialTextures; ++i ) {
			textures[ i ] = INVALID_HDL;
		}
		for ( int i = 0; i < MaxMaterialShaders; ++i ) {
			shaders[ i ] = INVALID_HDL;
			shaderPerms[ i ] = 0;
		}
		uploadId = -1;
		usage = MATERIAL_USAGE_UNKNOWN;

		extraByteCount = 0;
		memset( extraData, 0, MaxExtraDataBytes );
	}

	inline void SetParms( const materialParms_t& parms )
	{
		p = parms;
	}

	inline void SetExtraData( void* data, const uint32_t byteCount )
	{
		extraByteCount = Min( MaxExtraDataBytes, byteCount );
		memcpy( extraData, data, extraByteCount );
	}

	inline void CopyExtraData( void* outData, const uint32_t byteCount )
	{
		if( ( byteCount == 0 ) || ( extraByteCount == 0 ) ) {
			return;
		}
		memcpy( outData, extraData, Min( byteCount, extraByteCount ) );
	}

	inline uint32_t GetExtraDataByteCount() const
	{
		return extraByteCount;
	}

	inline const materialParms_t& GetParms() const
	{
		return p;
	}

	inline materialParms_t& GetParms()
	{
		return p;
	}

	inline bool IsTextured() const
	{
		return ( textureBitSet > 0 );
	}

	bool		AddTexture( const uint32_t slot, const hdl_t hdl );
	hdl_t		GetTexture( const uint32_t slot ) const;
	uint32_t	TextureCount() const;
	bool		AddShader( const drawPass_t pass, const hdl_t hdl, const uint32_t perms = 0 );
	hdl_t		GetShader( const drawPass_t pass ) const;
	uint32_t	GetShaderPerms( const drawPass_t pass ) const;
	uint32_t	ShaderCount() const;

	void Serialize( Serializer* serializer );
};

float GGX( float NoH, float roughness );
float SmithGGXCorrelated( float NoV, float NoL, float roughness );
vec3f Schlick( float u, vec3f f0 );
float Lambert();
vec3f BrdfGGX( const vec3f& n, const vec3f& v, const vec3f& l, const Material& m );

class AssetManager;

class BakedMaterialLoader : public LoadHandler<Material>
{
private:
	std::string		m_assetDir;
	std::string		m_textureDir;
	std::string		m_bakedDir;
	std::string		m_fileName;
	std::string		m_ext;
	AssetManager*	m_assets;

	bool Load( Asset<Material>& texture );

public:
	BakedMaterialLoader() {}
	BakedMaterialLoader( AssetManager* assets, const std::string& path, const std::string& ext )
	{
		SetAssetRef( assets );
		SetAssetPath( path );
		SetExtName( ext );
	}

	void SetAssetPath( const std::string& path );
	void SetExtName( const std::string& file );
	void SetAssetRef( AssetManager* assetsPtr );
};

class MaterialLoader : public LoadHandler<Material>
{
private:
	std::string		m_materialPath;
	std::string		m_texturePath;
	std::string		m_fileName;
	AssetManager*	m_assets;

	bool Load( Asset<Material>& materialAsset );

public:
	MaterialLoader() : m_assets( nullptr ) {}

	void SetMaterialPath( const std::string& path );
	void SetTexturePath( const std::string& path );
	void SetFileName( const std::string& fileName );
	void SetAssetRef( AssetManager* assetsPtr );
};

using pMatLoader_t = Asset<Material>::loadHandlerPtr_t;
