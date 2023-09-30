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
#include "../render_core/GpuSync.h"

class ShaderBindParms;
class GpuBuffer;

enum gpuImageStateFlags_t : uint8_t;
enum downSampleMode_t : uint32_t;

enum pipelineQueue_t
{
	QUEUE_UNKNOWN,
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


class CommandContext
{
protected:
	pipelineQueue_t				queueType;
	bool						isOpen;
private:
	std::vector<GpuSemaphore*>	waitSemaphores;
	std::vector<GpuSemaphore*>	signalSemaphores;
#ifdef USE_VULKAN
	VkCommandPool				commandPool;
	VkCommandBuffer				commandBuffers[ MaxFrameStates ];
#endif
public:
	CommandContext()
	{
#ifdef USE_VULKAN
		commandPool = VK_NULL_HANDLE;
		for( uint32_t i = 0; i < MaxFrameStates; ++i ) {
			commandBuffers[ i ] = VK_NULL_HANDLE;
		}

		queueType = QUEUE_UNKNOWN;
		isOpen = false;
#endif
	}
#ifdef USE_VULKAN
	VkCommandBuffer&			CommandBuffer();
#endif
	void						Begin();
	void						End();
	void						Create( const char* name );
	void						Destroy();
	void						MarkerBeginRegion( const char* pMarkerName, const vec4f& color );
	void						MarkerEndRegion();
	void						MarkerInsert( std::string markerName, const vec4f& color );
	void						Wait( GpuSemaphore* semaphore );
	void						Signal( GpuSemaphore* semaphore );
	void						Submit( const GpuFence* fence = nullptr );
};


class GfxContext : public CommandContext
{
public:
	GpuSemaphore				presentSemaphore;
	GpuSemaphore				renderFinishedSemaphore;
	GpuFence					frameFence[ MaxFrameStates ];

	GfxContext()
	{
		queueType = QUEUE_GRAPHICS;
	}

	//void Submit( ); // TODO
	//void Dispatch( ); // TODO: Compute jobs can still be dispatched from gfx
};


class ComputeContext : public CommandContext
{
public:
	GpuSemaphore semaphore;

	ComputeContext()
	{
		queueType = QUEUE_COMPUTE;
	}

	void Submit();
	void Dispatch( const hdl_t progHdl, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y = 1, const uint32_t z = 1 );
};


class UploadContext : public CommandContext
{
public:
	UploadContext()
	{
		queueType = QUEUE_GRAPHICS;
	}
};


void Transition( CommandContext* cmdCommand, Image& image, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void GenerateMipmaps( CommandContext* cmdCommand, Image& image );
void CopyImage( CommandContext* cmdCommand, Image& src, Image& dst );
void CopyBufferToImage( CommandContext* cmdCommand, Image& texture, GpuBuffer& buffer, const uint64_t bufferOffset );
void GenerateDownsampleMips( CommandContext* cmdCommand, Image& image, downSampleMode_t mode );