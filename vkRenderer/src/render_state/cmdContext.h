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

class ShaderBindParms;

enum pipelineQueue_t
{
	QUEUE_GRAPHICS,
	QUEUE_PRESENT,
	QUEUE_COMPUTE,
	QUEUE_COUNT,
};


template<class T>
class optional {

	std::pair<T, bool> option;

public:

	optional()
	{
		option.second = false;
	}

	bool has_value()
	{
		return option.second;
	}

	void set_value( const T& value_ )
	{
		option.first = value_;
		option.second = true;
	}

	T value()
	{
		return option.first;
	}
};


struct QueueFamilyIndices
{
	optional<uint32_t> graphicsFamily;
	optional<uint32_t> presentFamily;
	optional<uint32_t> computeFamily;

	bool IsComplete() {
		return	graphicsFamily.has_value() &&
			presentFamily.has_value() &&
			computeFamily.has_value();
	}
};

class GfxContext
{
public:
	VkQueue						gfxContext;
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffers[ MAX_FRAMES_STATES ];
	VkSemaphore					imageAvailableSemaphores[ MAX_FRAMES_STATES ];
	VkSemaphore					renderFinishedSemaphores[ MAX_FRAMES_STATES ];
	VkFence						inFlightFences[ MAX_FRAMES_STATES ];
	VkFence						imagesInFlight[ MAX_FRAMES_STATES ];

	//void Submit( ); // TODO
	//void Dispatch( ); // TODO: Compute jobs can still be dispatched from gfx
};


class ComputeContext
{
public:
	VkQueue						queue;
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffers[ MAX_FRAMES_STATES ];
	VkSemaphore					semaphores[ MAX_FRAMES_STATES ];

	void Submit( const uint32_t bufferId );
	void Dispatch( const hdl_t progHdl, const uint32_t bufferId, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y = 1, const uint32_t z = 1 );
};


class UploadContext
{
public:
	VkQueue						queue;
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffer;
};