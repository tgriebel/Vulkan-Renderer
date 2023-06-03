#pragma once
#include <GfxCore/asset_types/texture.h>
#include "../render_core/gpuImage.h"

class ImageView : public Image
{
private:
	void Init( const Image& image )
	{
		info = image.info;
		gpuImage = new GpuImage();
#ifdef USE_VULKAN
		gpuImage->VkImage() = image.gpuImage->GetVkImage();
		gpuImage->VkImageView() = image.gpuImage->GetVkImageView();
#endif
	}

public:
	ImageView() : Image()
	{}


	ImageView( const Image& image ) : Image()
	{
		Init( image );
	}

	
	~ImageView()
	{
		if( gpuImage != nullptr )
		{
			delete gpuImage;
			gpuImage = nullptr;
		}
	}


	ImageView& operator=( const Image& image )
	{
		Init( image );
		return *this;
	}
};