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
		camera = Camera( glm::vec4( 0.0f, 1.66f, 1.0f, 0.0f ) );
		camera.fov = glm::radians( 90.0f );
		camera.far = defaultFar;
		camera.near = defaultNear;
		camera.aspect = 16.0f / 9.0f;
	}

	void CreateEntity( const uint32_t modelId, entity_t& entity )
	{
		const modelSource_t* model = modelLib.Find( modelId );

		entity.modelId = modelId;
		entity.matrix = glm::identity<glm::mat4>();
		entity.flags = renderFlags_t::NONE;
		entity.materialId = -1;
	}
};