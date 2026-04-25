#pragma once

#include <cstdint>

#include "../render_resources/gpuBuffer.h"

class RenderContext;
class ResourceContext;


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
public:
	using surfUploadArray_t = Array<surfaceUpload_t, MaxSurfaces* MaxViews>;

	GpuBuffer			stagingBuffer;
	GpuBuffer			vb;
	GpuBuffer			ib;
	surfUploadArray_t	surfUploads;

	uint32_t			vbBufElements = 0;
	uint32_t			ibBufElements = 0;
};


class RenderUploader
{
private:
	uint32_t			imageFreeSlot;
	GpuBuffer			textureStagingBuffer;

	RenderContext*		renderContext;
	ResourceContext*	resourceContext;

	std::set<hdl_t>		uploadTextures;
	std::set<hdl_t>		updateTextures;

public:
	void				Boot( RenderContext* context, ResourceContext* resources );
	void				Shutdown();

	void				UploadImage( Asset<Image>* imageAsset );

	void				UpdateTextureData( CommandContext* cmdCommand );
	void				UploadTextures( CommandContext* cmdCommand );
};