#pragma once



#include "../globals/common.h"
#include "../render_core/GpuSync.h"

class ShaderBindParms;
class GpuBuffer;
class RenderContext;
class DrawPass;
class ImageView;
class GpuImage;
class FrameBuffer;
struct imageSubResourceView_t;
struct copyImageParms_t;

enum gpuImageStateFlags_t : uint8_t;

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
	RenderContext*				m_renderContext;

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
	void						Create( const char* name, RenderContext* context );
	void						Destroy();
	void						MarkerBeginRegion( const char* pMarkerName, const vec4f& color );
	void						MarkerEndRegion();
	void						MarkerInsert( std::string markerName, const vec4f& color );
	void						Wait( GpuSemaphore* semaphore );
	void						Signal( GpuSemaphore* semaphore );
	void						Submit( const GpuFence* fence = nullptr );
	void						Dispatch( const hdl_t progHdl, const ShaderBindParms& bindParms, const uint32_t x, const uint32_t y, const uint32_t z );
	void						Dispatch( const hdl_t progHdl, const ShaderBindParms& bindParms, const void* constants, const uint32_t constantsSize, const uint32_t x, const uint32_t y, const uint32_t z );

	inline RenderContext*	GetRenderContext()
	{
		return m_renderContext;
	}

	inline const RenderContext* GetRenderContext() const
	{
		return m_renderContext;
	}
};


class GfxCmdList : public CommandContext
{
public:
	GpuSemaphore	presentSemaphore;
	GpuSemaphore	renderFinishedSemaphore;
	GpuFence		frameFence[ MaxFrameStates ];

	GfxCmdList()
	{
		queueType = QUEUE_GRAPHICS;
	}
};


class ComputeCmdList : public CommandContext
{
public:
	GpuSemaphore	semaphore;

	ComputeCmdList()
	{
		queueType = QUEUE_COMPUTE;
	}
};


class UploadCmdList : public CommandContext
{
private:
	using CommandContext::Dispatch;

public:
	UploadCmdList()
	{
		queueType = QUEUE_GRAPHICS;
	}
};


void Transition( CommandContext* cmdCommand, const Image& image, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void Transition( CommandContext* cmdCommand, const Image& image, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void Transition( CommandContext* cmdCommand, const GpuImage* gpuImage, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void GenerateMipmaps( CommandContext* cmdCommand, Image& image );
void CopyImage( CommandContext* cmdCommand, Image& src, Image& dst );
void CopyImage( CommandContext* cmdCommand, Image& src, const copyImageParms_t& srcParms, Image& dst, const copyImageParms_t& dstParms );
void UploadImageData( CommandContext* cmdCommand, Image& image, imageSubResourceView_t& subView, GpuBuffer& buffer );
void FlushGPU();