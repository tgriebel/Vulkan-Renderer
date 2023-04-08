#include "gpuImage.h"
#include "../render_state/deviceContext.h"

void GpuImage::Destroy()
{
	assert( context.device != VK_NULL_HANDLE );
	if ( context.device != VK_NULL_HANDLE )
	{
		if( vk_view != VK_NULL_HANDLE ) {
			vkDestroyImageView( context.device, vk_view, nullptr );
			vk_view = VK_NULL_HANDLE;
		}
		if ( vk_image != VK_NULL_HANDLE ) {
			vkDestroyImage( context.device, vk_image, nullptr );
			vk_image = VK_NULL_HANDLE;
		}
		allocation.Free();
	}
}