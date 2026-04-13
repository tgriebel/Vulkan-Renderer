#pragma once

#include <cinttypes>
#include <vector>
#include <GfxCore/acceleration/aabb.h>
#include <GfxCore/primitives/geom.h>
#include <SysCore/handle.h>

#include "asset.h"
#include "../io/io.h"

class Surface {
public:
	hdl_t						materialHdl;
	std::vector<vertex_t>		vertices;
	std::vector<uint32_t>		indices;
	vec3f						centroid;

	void Serialize( Serializer* serializer );
};


class Model
{
	static const uint32_t Version = 1;
public:
	Model() : surfCount( 0 ), uploadId( -1 )
	{
	}

	AABB						bounds;
	std::vector<Surface>		surfs;
	int32_t						uploadId;
	uint32_t					surfCount;

	void Serialize( Serializer* serializer );
};


class ModelLoader : public LoadHandler<Model>
{
private:
	std::string		m_texturePath;
	std::string		m_modelPath;
	std::string		m_modelName;
	std::string		m_modelExt;
	std::string		m_materialDir;
	std::string		m_materialExt;
	std::string		m_bakedDir;
	AssetManager*	assets;

	bool Load( Asset<Model>& modelAsset );

public:
	void SetTexturePath( const std::string& path );
	void SetModelPath( const std::string& path );
	void SetModelName( const std::string& fileName );
	void SetAssetRef( AssetManager* assetsPtr );
};

using loader_t = Asset<Model>::loadHandlerPtr_t;
