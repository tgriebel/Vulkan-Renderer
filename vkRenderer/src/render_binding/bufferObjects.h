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


struct globalUboConstants_t
{
	vec4f		time;
	vec4f		generic;
	vec4f		shadowParms;
	vec4f		toneMapTint;
	vec4f		bloom;
	vec4f		exposure;
	vec4f		exposure2;
	vec4f		dof;
	uint32_t	numSamples;
	uint32_t	whiteId;
	uint32_t	blackId;
	uint32_t	defaultAlbedoId;
	uint32_t	defaultNormalId;
	uint32_t	defaultRoughnessId;
	uint32_t	defaultMetalId;
	uint32_t	defaultImageId;
	uint32_t	brdfLutId;
	uint32_t	isTextured;
	uint32_t	shadow2dCount;
	uint32_t	shadowCubeCount;
	uint32_t	textureCount;
	uint32_t	materialCount;
	// uint32_t	pad[ 4 ]; // minUniformBufferOffsetAlignment
};


struct materialBufferObject_t
{
	int						textures[ Material::MaxMaterialTextures ];
	vec3f					Ka;
	float					Tr;
	vec3f					Ke;
	float					Ns;
	vec3f					Kd;
	float					Ni;
	vec3f					Ks;
	float					illum;
	vec3f					Tf;
	uint32_t				textured;
	uint32_t				pad[ 4 ]; // Multiple of minUniformBufferOffsetAlignment (0x40)
	uint8_t					extra[ Material::MaxExtraDataBytes ];
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