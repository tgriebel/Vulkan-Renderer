#pragma once



#include "../globals/common.h"

struct vsInput_t
{
	vec3f pos;
	vec4f color;
	vec3f normal;
	vec3f tangent;
	vec3f bitangent;
	vec4f texCoord;
};


struct surfaceBufferObject_t
{
	mat4x4f		model;
	uint32_t	diffuseIblCubeId;
	uint32_t	envCubeId;
	uint32_t	pad[ 14 ];
};


struct viewBufferObject_t
{
	mat4x4f		view;
	mat4x4f		proj;
	vec4f		dimensions;
	vec3f		viewOrigin;
	uint32_t	numLights;
};


struct lightBufferObject_t
{
	vec4f		lightPos;
	vec4f		intensity;
	vec4f		lightDir;
	uint32_t	shadowViewId;
	uint32_t	pad[ 3 ];
};


struct particleBufferObject_t
{
	vec2f	position;
	vec2f	velocity;
	vec4f	color;
};


struct imageProcessObject_t
{
	vec4f	dimensions;
	vec4f	generic0;
	vec4f	generic1;
	vec4f	generic2;
};
