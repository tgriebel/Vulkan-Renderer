#pragma once
#include "common.h"
#include "camera.h"
#include "assetLib.h"
#include "window.h"

typedef AssetLib< modelSource_t >	AssetLibModels;
typedef AssetLib< texture_t >		AssetLibImages;
typedef AssetLib< Material >		AssetLibMaterials;
typedef AssetLib< GpuProgram >		AssetLibGpuProgram;
extern Window window;

struct Scene
{
	Camera						camera;
	AssetLib< Entity* >			entities;
	AssetLibModels				modelLib;
	AssetLibImages				textureLib;
	AssetLibMaterials			materialLib;
	AssetLibGpuProgram			gpuPrograms;
	light_t						lights[ MaxLights ];
	float						defaultNear = 1000.0f;
	float						defaultFar = 0.1f;

	Scene()
	{
		camera = Camera( vec4f( 0.0f, 1.66f, 1.0f, 0.0f ) );
		camera.fov = Radians( 90.0f );
		camera.far = defaultFar;
		camera.near = defaultNear;
		camera.aspect = DEFAULT_DISPLAY_WIDTH / static_cast< float >( DEFAULT_DISPLAY_HEIGHT );

		camera.halfFovX = tan( 0.5f * Radians( 90.0f ) );
		camera.halfFovY = tan( 0.5f * Radians( 90.0f ) ) / camera.aspect;
		camera.near = camera.near;
		camera.far = camera.far;
		camera.aspect = camera.aspect;
		camera.focalLength = camera.far;
		camera.viewportWidth = 2.0f * camera.halfFovX;
		camera.viewportHeight = 2.0f * camera.halfFovY;
	}

	void CreateEntity( const int32_t modelId, Entity& entity )
	{
		const modelSource_t* model = modelLib.Find( modelId );
		entity.modelId = modelId;
	}

	Entity* FindEntity( const int entityId ) {
		return *entities.Find( entityId );
	}

	Entity* FindEntity( const char* name ) {
		return *entities.Find( name );
	}
};