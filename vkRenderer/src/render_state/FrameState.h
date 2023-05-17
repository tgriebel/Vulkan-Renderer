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

#pragma once

#include "../globals/common.h"
#include "../render_binding/gpuResources.h"
#include "../render_core/gpuImage.h"
#include <resource_types/texture.h>

class FrameState
{
public:
	Texture			viewColorImage;
	Texture			shadowMapImage;
	Texture			depthImage;
	Texture			stencilImage;

	// FIXME: Lights and surfaces should be relative to a given render view.
	// Textures, materials, and view parms are all global

	GpuBuffer		globalConstants;
	GpuBuffer		viewParms;
	GpuBuffer		surfParms;
	GpuBuffer		materialBuffers;
	GpuBuffer		lightParms;
	GpuBuffer		particleBuffer;

	GpuBufferView	surfParmPartitions[ MaxViews ]; // "View" is used in two ways here: view of data, and view of scene
};


struct frameBufferCreateInfo_t
{
	uint32_t	width;
	uint32_t	height;
	Texture*	color0;
	Texture*	color1;
	Texture*	color2;
	Texture*	depth;
	Texture*	stencil;

	frameBufferCreateInfo_t() :
		width( 0 ),
		height( 0 ),
		color0( nullptr ),
		color1( nullptr ),
		color2( nullptr ),
		depth( nullptr ),
		stencil( nullptr )
	{}
};


class FrameBuffer
{
public:
	Texture*		color0;
	Texture*		color1;
	Texture*		color2;
	Texture*		depth;
	Texture*		stencil;

#ifdef USE_VULKAN
	VkFramebuffer	buffer;
	VkRenderPass	renderPass;
#endif

	uint32_t		width;
	uint32_t		height;

	FrameBuffer()
	{
		width = 0;
		height = 0;
	}

	inline bool IsValid() const
	{
		return ( buffer != VK_NULL_HANDLE );
	}

	inline uint32_t GetWidth()
	{
		return width;
	}

	inline uint32_t GetHeight()
	{
		return height;
	}

#ifdef USE_VULKAN
	VkFramebuffer& VkObject()
	{
		return buffer;
	}

	VkFramebuffer GetVkObject() const
	{
		return buffer;
	}
#endif

	void Create( const frameBufferCreateInfo_t& createInfo );
	void Destroy();
};