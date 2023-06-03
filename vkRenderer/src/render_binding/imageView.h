#pragma once
#include <GfxCore/asset_types/texture.h>
#include "../render_core/gpuImage.h"

class ImageView : public Image
{
private:

public:
	ImageView( const Image* image ) : Image()
	{
		
		info = image->info;
		gpuImage = new GpuImage();
#ifdef USE_VULKAN
		gpuImage->VkImage() = image->gpuImage->GetVkImage();
		gpuImage->VkImageView() = image->gpuImage->GetVkImageView();
#endif
	}

	~ImageView()
	{
		if( gpuImage != nullptr )
		{
			delete gpuImage;
			gpuImage = nullptr;
		}
	}
};