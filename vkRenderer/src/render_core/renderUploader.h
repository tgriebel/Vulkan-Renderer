#pragma once

#include <cstdint>

#include "../render_resources/gpuBuffer.h"

class RenderContext;
class ResourceContext;

class RenderUploader
{
private:
	uint32_t			imageFreeSlot = 0;
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