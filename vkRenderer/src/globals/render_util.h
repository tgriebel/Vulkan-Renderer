#pragma once

#include "common.h"

#include "../asset_types/asset.h"
#include "../asset_types/model.h"

class SkyBoxLoader : public LoadHandler<Model>
{
private:
	bool Load( Asset<Model>& model );
public:
};


class TerrainLoader : public LoadHandler<Model>
{
private:
	int width;
	int height;
	float cellSize;
	float uvScale;
	hdl_t handle;
	bool Load( Asset<Model>& model );
public:
	TerrainLoader( const int _width, const int _height, const float _cellSize, const float _uvScale, const hdl_t _handle )
	{
		width = _width;
		height= _height;
		cellSize = _cellSize;
		handle = _handle;
		uvScale = _uvScale;
	}
};


class WaterLoader : public LoadHandler<Model>
{
private:
	bool Load( Asset<Model>& model );
public:
};


enum class modelGenType_t : uint32_t
{
	PLANE = 0,
	SPHERE = 1,
	BOX = 2,
};


class ModelGenLoader : public LoadHandler<Model>
{
private:
	bool Load( Asset<Model>& model );
public:
};

mat4x4f MatrixFromVector( const vec3f& v );
void MatrixToEulerZYX( const mat4x4f& m, float& a, float& b, float& c );
void CreateQuadSurface2D( Model& outModel, const std::string& materialName, vec2f& origin, vec2f& size );
void CreateBoxSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float size );
void CreateSphereSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float radius );
