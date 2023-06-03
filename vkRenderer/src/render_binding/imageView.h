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
			gpuImage->DetachVkImage();
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
#ifdef USE_VULKAN
		const VkImage img = image.gpuImage->GetVkImage();
		const VkImageView view = vk_CreateImageView( image.gpuImage->GetVkImage(), info );

		gpuImage = new GpuImage( img, view );
#endif
	}
};