#pragma once
#include "common.h"
#include "camera.h"
#include "assetLib.h"

extern AssetLib< modelSource_t > modelLib;

struct Scene
{
	Camera						camera;
	AssetLib< entity_t >		entities;
	light_t						lights[ MaxLights ];
	float						defaultNear = 1000.0f;
	float						defaultFar = 0.1f;

	Scene()
	{
		camera = Camera( vec4f( 0.0f, 1.66f, 1.0f, 0.0f ) );
		camera.fov = glm::radians( 90.0f );
		camera.far = defaultFar;
		camera.near = defaultNear;
		camera.aspect = 16.0f / 9.0f;

		camera.halfFovX = tan( 0.5f * Radians( 90.0f ) );
		camera.halfFovY = tan( 0.5f * Radians( 90.0f ) ) / camera.aspect;
		camera.near = camera.near;
		camera.far = camera.far;
		camera.aspect = camera.aspect;
		camera.focalLength = camera.far;
		camera.viewportWidth = 2.0f * camera.halfFovX;
		camera.viewportHeight = 2.0f * camera.halfFovY;
	}

	void CreateEntity( const uint32_t modelId, entity_t& entity )
	{
		const modelSource_t* model = modelLib.Find( modelId );

		entity.modelId = modelId;
		entity.matrix = mat4x4f( 1.0f );
		entity.flags = renderFlags_t::NONE;
		entity.materialId = -1;
	}
};