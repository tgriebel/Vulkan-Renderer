#pragma once

/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

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
	mat4x4f model;
};


struct viewBufferObject_t
{
	mat4x4f		view;
	mat4x4f		proj;
	vec4f		dimensions;
	uint32_t	numLights;
};


struct globalUboConstants_t
{
	vec4f		time;
	vec4f		generic;
	vec4f		shadowParms;
	vec4f		tonemap;
	uint32_t	numSamples;
	uint32_t	whiteId;
	uint32_t	blackId;
	uint32_t	isTextured;
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