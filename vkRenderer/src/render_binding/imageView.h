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


	ImageView( const Image& image, const imageInfo_t& imageInfo, const imageSubResourceView_t& subResourceView ) : Image()
	{
		Init( image, imageInfo, subResourceView );
	}

	
	~ImageView()
	{
		Destroy();
	}


	ImageView& operator=( const ImageView& image )
	{
		Init( image, image.info );
		return *this;
	}


	void Init( const Image& image, const imageInfo_t& imageInfo )
	{
		imageSubResourceView_t subView;
		subView.baseMip = 0;
		subView.mipLevels = imageInfo.mipLevels;
		subView.baseArray = 0;
		subView.arrayCount = imageInfo.layers;
		assert( subView.mipLevels >= 1 );
		assert( subView.arrayCount >= 1 );

		Init( image, imageInfo, subView );
	}


	void Init( const Image& image, const imageInfo_t& imageInfo, const imageSubResourceView_t& subView )
	{
		info = imageInfo;
		assert( info.layers >= 1 );
		assert( info.mipLevels >= 1 );

		MipDimensions( subView.baseMip, info.width, info.height, &info.width, &info.height );

		subResourceView = subView;
#ifdef USE_VULKAN
		const VkImage img = image.gpuImage->GetVkImage();
		const VkImageView view = vk_CreateImageView( image.gpuImage->GetVkImage(), info, subResourceView );

		gpuImage = new GpuImage( image.gpuImage->GetDebugName(), img, view );
#endif
	}


	void Destroy()
	{
		if ( gpuImage != nullptr )
		{
#ifdef USE_VULKAN
			// Views do not own the image, only the view
			gpuImage->DetachVkImage();
#endif
			delete gpuImage;
			gpuImage = nullptr;
		}
	}
};