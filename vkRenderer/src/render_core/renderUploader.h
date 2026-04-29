#pragma once

#include <cstdint>

#include "../render_resources/gpuBuffer.h"

class RenderContext;
class ResourceContext;
class Model;
class RenderUploader;

struct surfaceUpload_t
{
	surfaceUpload_t() : vertexCount( 0 ), indexCount( 0 ), vertexOffset( 0 ), firstIndex( 0 )
	{}

	uint32_t					vertexCount;
	uint32_t					indexCount;
	uint32_t					vertexOffset;
	uint32_t					firstIndex;
};


// Bundle of all resources needed to represent geometry on the GPU
class GeometryContext
{
private:

	using surfUploadArray_t = Array<surfaceUpload_t, MaxSurfaces* MaxViews>;

	GpuBuffer			stagingBuffer;
	uint32_t			vbBufElements = 0;
	uint32_t			ibBufElements = 0;

public:
	GpuBuffer			vb;
	GpuBuffer			ib;
	surfUploadArray_t	surfUploads;

	friend RenderUploader;
};


class RenderUploader
{
private:
	uint32_t			imageFreeSlot;
	GpuBuffer			textureStagingBuffer;
	GeometryContext		geometry;

	RenderContext*		renderContext;
	ResourceContext*	resourceContext;

	std::set<hdl_t>		uploadImages;
	std::set<hdl_t>		refreshImages;

#ifdef USE_VULKAN
	void				CopyGpuBuffer( CommandContext* cmdCommand, GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );
#endif

public:

	void				Boot( RenderContext* context, ResourceContext* resources );
	void				Shutdown();

	void				QueueModelUpload( Asset<Model>& model );

	void				UploadImage( Asset<Image>* imageAsset );

	void				UpdateTextureData( CommandContext* cmdCommand );
	void				UploadTextures( CommandContext* cmdCommand );
	void				UploadModelsToGPU( CommandContext* cmdCommand );

	inline const GeometryContext* GetGeometry() const
	{
		return &geometry;
	}
};