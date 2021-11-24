#pragma once
#include "common.h"
#include "camera.h"

struct Scene
{
	Camera						camera;
	std::vector<entity_t>		entities;
	std::vector<modelSource_t>	models;
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
};