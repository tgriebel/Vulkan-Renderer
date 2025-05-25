#pragma once
#include <GfxCore/asset_types/texture.h>
#include "../render_core/gpuImage.h"
#include "../render_state/deviceContext.h"

class ImageView : public Image, public RenderResource
{
public:
	ImageView() : Image()
	{}

	ImageView( const Image& _image ) = delete;

	ImageView( const ImageView& image ) : Image()
	{
		Init( image, image.info, image.m_lifetime );
	}


	ImageView( const Image& image, const imageInfo_t& imageInfo, const resourceLifeTime_t lifetime ) : Image()
	{
		Init( image, imageInfo, lifetime );
	}


	ImageView( const Image& image, const imageInfo_t& imageInfo, const imageSubResourceView_t& subResourceView, const resourceLifeTime_t lifetime ) : Image()
	{
		Init( image, imageInfo, subResourceView, lifetime );
	}

	
	~ImageView()
	{
		Destroy();
	}


	ImageView& operator=( const ImageView& image )
	{
		Init( image, image.info, image.m_lifetime );
		return *this;
	}


	void Init( const Image& image, const imageInfo_t& imageInfo, const resourceLifeTime_t lifetime )
	{
		imageSubResourceView_t subView;
		subView.baseMip = 0;
		subView.mipLevels = imageInfo.mipLevels;
		subView.baseArray = 0;
		subView.arrayCount = imageInfo.layers;
		assert( subView.mipLevels >= 1 );
		assert( subView.arrayCount >= 1 );

		Init( image, imageInfo, subView, lifetime );
	}


	void Init( const Image& image, const imageInfo_t& imageInfo, const imageSubResourceView_t& subView, const resourceLifeTime_t lifetime )
	{
		// Manage Resources
		{
			m_lifetime = lifetime;
			RenderResource::Create( m_lifetime );
		}

		info = imageInfo;
		assert( info.layers >= 1 );
		assert( info.mipLevels >= 1 );

		MipDimensions( subView.baseMip, info.width, info.height, &info.width, &info.height );

		subResourceView = subView;
#ifdef USE_VULKAN
		const VkImage img = image.gpuImage->GetVkImage( 0 );
		const VkImageView view = vk_CreateImageView( image.gpuImage->GetVkImage( 0 ), info, subResourceView );

		gpuImage = new GpuImage( image.gpuImage->GetDebugName(), img, view );

		assert( gpuImage->GetBufferCount() == 1 );
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