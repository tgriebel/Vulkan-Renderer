#pragma once
#include <GfxCore/asset_types/texture.h>
#include "../render_core/gpuImage.h"
#include "../render_state/deviceContext.h"

class ImageView : public Image
{
public:
	ImageView() : Image()
	{}

	ImageView( const Image& _image ) = delete;

	ImageView( const ImageView& image ) : Image()
	{
		Init( image, image.info );
	}


	ImageView( const Image& image, const imageInfo_t& imageInfo ) : Image()
	{
		Init( image, imageInfo );
	}

	
	~ImageView()
	{
		if( gpuImage != nullptr )
		{
#ifdef USE_VULKAN
			// Views do not own the image, only the view
			gpuImage->VkImage() = VK_NULL_HANDLE;
#endif
			delete gpuImage;
			gpuImage = nullptr;
		}
	}


	ImageView& operator=( const ImageView& image )
	{
		Init( image, image.info );
		return *this;
	}


	void Init( const Image& image, const imageInfo_t& imageInfo )
	{
		info = imageInfo;
		gpuImage = new GpuImage();
#ifdef USE_VULKAN
		gpuImage->VkImage() = image.gpuImage->GetVkImage();
		gpuImage->VkImageView() = vk_CreateImageView( gpuImage->GetVkImage(), info );
#endif
	}
};