#include "deviceContext.h"
#include "../render_core/swapChain.h"
#include "../draw_passes/drawpass.h"
#include "../render_core/gpuImage.h"
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"

DeviceContext context;


static bool IsRenderDocAttached()
{
#if defined( _WIN32 )
	// RenderDoc injects renderdoc.dll into the target process before Vulkan loads.
	// Its timestamp-query handling is flaky during capture, so disable our pool
	// when it's present.
	return GetModuleHandleA( "renderdoc.dll" ) != nullptr;
#else
	return false;
#endif
}


bool vk_CheckDeviceExtensionSupport( VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions )
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );

	std::vector<VkExtensionProperties> availableExtensions( extensionCount );
	vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

	std::set<std::string> requiredExtensions( deviceExtensions.begin(), deviceExtensions.end() );

	for ( const auto& extension : availableExtensions ) {
		requiredExtensions.erase( extension.extensionName );
	}
	return requiredExtensions.empty();
}


bool vk_IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface,  const std::vector<const char*>& deviceExtensions )
{
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties( device, &deviceProperties );

	QueueFamilyIndices indices = vk_FindQueueFamilies( device, surface );

	bool extensionsSupported = vk_CheckDeviceExtensionSupport( device, deviceExtensions );

	bool swapChainAdequate = false;
	if ( extensionsSupported )
	{
		swapChainInfo_t swapChainSupport = SwapChain::QuerySwapChainSupport( device, surface );
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures( device, &supportedFeatures );

	return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}


QueueFamilyIndices vk_FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface )
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

	std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

	int i = 0;
	for ( const auto& queueFamily : queueFamilies )
	{
		if ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
			indices.graphicsFamily.set_value( i );
		}

		if ( queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT ) {
			indices.computeFamily.set_value( i );
		}

		VkBool32 presentSupport = false;

		vkGetPhysicalDeviceSurfaceSupportKHR( device, i, surface, &presentSupport );
		if ( presentSupport ) {
			indices.presentFamily.set_value( i );
		}

		if ( indices.IsComplete() ) {
			break;
		}

		i++;
	}

	return indices;
}


bool vk_ValidTextureFormat( const VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features )
{
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties( context.physicalDevice, format, &props );

	if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) {
		return true;
	}
	else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) {
		return true;
	}
	return false;
}


uint32_t vk_FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties )
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
	{
		if ( typeFilter & ( 1 << i ) && ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties )
		{
			return i;
		}
	}

	throw std::runtime_error( "Failed to find suitable memory type!" );
}


int32_t vk_MapToGlslCubemapConvention( const uint32_t index )
{
	static const int32_t glslCubeMapping[ 6 ] = { 4, 5, 1, 0, 2, 3 };
	return glslCubeMapping[ index ];
}


VkImageView vk_CreateImageView( const VkImage image, const imageInfo_t& info, const char* debugName, const uint32_t debugBufferId )
{
	imageSubResourceView_t subResourceView;
	subResourceView.baseMip = 0;
	subResourceView.mipLevels = info.mipLevels;
	subResourceView.baseArray = 0;
	subResourceView.arrayCount = info.layers;
	subResourceView.aspect = GetColorAspectFlags( info.fmt );

	return vk_CreateImageView( image, info, subResourceView, debugName, debugBufferId );
}


VkImageView vk_CreateImageView( const VkImage image, const imageInfo_t& info, const imageSubResourceView_t& subResourceView, const char* debugName, const uint32_t debugBufferId )
{
	const VkImageAspectFlags aspectFlags = vk_GetColorAspectFlags( info.fmt ) & vk_GetAspectFlags( subResourceView.aspect );

	assert( subResourceView.mipLevels >= 1 );
	assert( subResourceView.arrayCount >= 1 );

	VkImageViewCreateInfo viewInfo{ };
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = vk_GetImageViewType( info.type );
	viewInfo.format = vk_GetTextureFormat( info.fmt );
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = subResourceView.baseMip;
	viewInfo.subresourceRange.levelCount = subResourceView.mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = subResourceView.baseArray;
	viewInfo.subresourceRange.layerCount = ( info.type == IMAGE_TYPE_CUBE ) ? 6 : subResourceView.arrayCount;

	VkImageView imageView;
	VK_CHECK_RESULT( vkCreateImageView( context.device, &viewInfo, nullptr, &imageView ) );

	if( debugName != "" )
	{
		vk_SetObjectName( (uint64_t)imageView, VK_OBJECT_TYPE_IMAGE_VIEW, vk_BuildObjectName( "ImageView", debugName, debugBufferId ).c_str() );
	}

	return imageView;
}


void vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const Image* image, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	vk_TransitionImageLayout( cmdBuffer, image->gpuImage, subView, buffering, current, next );
}


void vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const GpuImage* gpuImage, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	VkImageMemoryBarrier barrier{ };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.baseMipLevel = subView.baseMip;
	barrier.subresourceRange.levelCount = subView.mipLevels;
	barrier.subresourceRange.baseArrayLayer = subView.baseArray;
	barrier.subresourceRange.layerCount = subView.arrayCount;

	const VkImageAspectFlagBits aspectFlags = vk_GetColorAspectFlags( gpuImage->GetInfo().fmt );

	const bool hasColorAspect = ( aspectFlags & VK_IMAGE_ASPECT_COLOR_BIT ) != 0;
	const bool hasDepthAspect = ( aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;
	const bool hasStencilAspect = ( aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT ) != 0;

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	barrier.srcAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( ( current & GPU_IMAGE_READ ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier.oldLayout = hasColorAspect ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
	else if ( ( current & GPU_IMAGE_PRESENT ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if ( ( current & GPU_IMAGE_TRANSFER_SRC ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if ( ( current & GPU_IMAGE_TRANSFER_DST ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else if( ( current & GPU_IMAGE_STORAGE ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	else if ( ( current & GPU_IMAGE_WRITE ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.oldLayout = hasColorAspect ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	barrier.dstAccessMask = 0;
	barrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( ( next & GPU_IMAGE_READ ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier.newLayout = hasColorAspect ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
	else if ( ( next & GPU_IMAGE_PRESENT ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if ( ( next & GPU_IMAGE_TRANSFER_SRC ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if ( ( next & GPU_IMAGE_TRANSFER_DST ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else if( ( next & GPU_IMAGE_STORAGE ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	else if ( ( next & GPU_IMAGE_WRITE ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.newLayout = hasColorAspect ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	barrier.subresourceRange.aspectMask = aspectFlags;

	if ( buffering == swapBuffering_t::SINGLE_FRAME )
	{
		barrier.image = gpuImage->GetVkImage( context.bufferId );
		vkCmdPipelineBarrier(
			cmdBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}
	else
	{
		const uint32_t bufferCount = gpuImage->GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			barrier.image = gpuImage->GetVkImage( i );
			vkCmdPipelineBarrier(
				cmdBuffer,
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
	}
}


void vk_GenerateMipmaps( VkCommandBuffer cmdBuffer, Image* image )
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( image->info.fmt ), &formatProperties );

	if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) )
	{
		throw std::runtime_error( "texture outputImage format does not support linear blitting!" );
	}

	VkImageAspectFlags aspectMask = vk_GetColorAspectFlags( image->info.fmt );

	VkImageMemoryBarrier barrier{ };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image->gpuImage->GetVkImage( context.bufferId );
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = image->subResourceView.arrayCount;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = image->info.width;
	int32_t mipHeight = image->info.height;

	for ( uint32_t i = 1; i < image->subResourceView.mipLevels; i++ )
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(	cmdBuffer,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier );

		uint32_t dstMipWidth;
		uint32_t dstMipHeight;
		MipDimensions( i, image->info.width, image->info.height, &dstMipWidth, &dstMipHeight );

		VkImageBlit blit{ };
		blit.srcOffsets[ 0 ] = { 0, 0, 0 };
		blit.srcOffsets[ 1 ] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = aspectMask;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = image->subResourceView.arrayCount;
		blit.dstOffsets[ 0 ] = { 0, 0, 0 };
		blit.dstOffsets[ 1 ] = { (int32_t)dstMipWidth, (int32_t)dstMipHeight, 1 };
		blit.dstSubresource.aspectMask = aspectMask;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = image->subResourceView.arrayCount;

		vkCmdBlitImage( cmdBuffer,
						image->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						image->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&blit,
						VK_FILTER_LINEAR );

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(	cmdBuffer,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier );

		mipWidth = static_cast<int32_t>( dstMipWidth );
		mipHeight = static_cast<int32_t>( dstMipHeight );
	}

	barrier.subresourceRange.baseMipLevel = image->subResourceView.mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(	cmdBuffer,
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &barrier );
}


void vk_QuadDraw( CommandList& cmdContext, const hdl_t pipeLineHandle, const vec4f& extent, const scissor_t& clipRect, const DrawPass* pass )
{
	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	const viewport_t& viewport = pass->GetViewport();
	
	VkViewport vk_viewport{ };
	// TODO: Fix UVs in shader before clamping
	//vk_viewport.x = Max( extent.x, (float)viewport.x );
	//vk_viewport.y = Max( extent.y, (float)viewport.y );
	//vk_viewport.width = Clamp( extent.z, extent.x, (float)viewport.width );
	//vk_viewport.height = Clamp( extent.w, extent.y, (float)viewport.height );
	vk_viewport.x = extent.x;
	vk_viewport.y = extent.y;
	vk_viewport.width = extent.z;
	vk_viewport.height = extent.w;
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

	const scissor_t& scissor = pass->GetScissor();

	VkRect2D rect{ };
	rect.offset.x = clipRect.x;
	rect.offset.y = clipRect.y;
	rect.extent.width = clipRect.width;
	rect.extent.height = clipRect.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipeLineHandle, &pipelineObject );

	if( pipelineObject->pipeline == VK_NULL_HANDLE ) {
		CreateGraphicsPipeline( pipeLineHandle, pipelineObject->state );
	}

	if ( pipelineObject != nullptr )
	{
		const uint32_t descSetCount = 2;
		VkDescriptorSet descSetArray[ descSetCount ] = { cmdContext.GetRenderContext()->globalParms->GetVkObject(), pass->parms->GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );

		vkCmdDraw( cmdBuffer, 3, 1, 0, 0 );
	}
}


void vk_RenderImageShader( CommandList& cmdContext, const hdl_t pipeLineHandle, const DrawPass* pass, const renderPassTransition_t& transitionState )
{
	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	const FrameBuffer* fb = pass->GetFrameBuffer();
	const uint32_t colorAttachmentsCount = fb->ColorLayerCount();

	const VkAttachmentLoadOp  loadOp  = transitionState.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	const VkAttachmentStoreOp storeOp = transitionState.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;

	const VkClearValue clearColor = {};

	const Image* colorImages[ 3 ] = { fb->GetColor(), fb->GetColor1(), fb->GetColor2() };
	VkRenderingAttachmentInfo colorAttachments[ 3 ] = {};
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i )
	{
		colorAttachments[ i ].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachments[ i ].imageView   = colorImages[ i ]->gpuImage->GetVkImageView( context.bufferId );
		colorAttachments[ i ].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachments[ i ].loadOp      = loadOp;
		colorAttachments[ i ].storeOp     = storeOp;
		colorAttachments[ i ].clearValue  = clearColor;
	}

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset    = { pass->GetViewport().x, pass->GetViewport().y };
	renderingInfo.renderArea.extent    = { pass->GetViewport().width, pass->GetViewport().height };
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = colorAttachmentsCount;
	renderingInfo.pColorAttachments    = colorAttachmentsCount > 0 ? colorAttachments : nullptr;

	// Transition render targets from their current layout into attachment-write layout
	const gpuImageStateFlags_t colorPriorState = transitionState.flags.presentBefore ? GPU_IMAGE_PRESENT : GPU_IMAGE_READ;
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
		Transition( &cmdContext, *colorImages[ i ], colorPriorState, GPU_IMAGE_WRITE );
	}

	vkCmdBeginRendering( cmdBuffer, &renderingInfo );

	const viewport_t& viewport = pass->GetViewport();

	VkViewport vk_viewport{ };
	vk_viewport.x = static_cast<float>( viewport.x );
	vk_viewport.y = static_cast<float>( viewport.y );
	vk_viewport.width = static_cast<float>( viewport.width );
	vk_viewport.height = static_cast<float>( viewport.height );
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

	VkRect2D rect{ };
	rect.extent.width = viewport.width;
	rect.extent.height = viewport.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipeLineHandle, &pipelineObject );

	if( pipelineObject->pipeline == VK_NULL_HANDLE ) {
		CreateGraphicsPipeline( pipeLineHandle, pipelineObject->state );
	}

	if ( pipelineObject != nullptr )
	{
		const uint32_t descSetCount = 2;
		VkDescriptorSet descSetArray[ descSetCount ] = { cmdContext.GetRenderContext()->globalParms->GetVkObject(), pass->parms->GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );

		vkCmdDraw( cmdBuffer, 3, 1, 0, 0 );
	}

	vkCmdEndRendering( cmdBuffer );

	// Transition render targets back for subsequent sampling (or presentation)
	const gpuImageStateFlags_t colorNextState = transitionState.flags.presentAfter ? GPU_IMAGE_PRESENT : GPU_IMAGE_READ;
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
		Transition( &cmdContext, *colorImages[ i ], GPU_IMAGE_WRITE, colorNextState );
	}
}


void vk_CopyImage( VkCommandBuffer cmdBuffer, const Image* src, const copyImageParms_t& srcParms, Image* dst, const copyImageParms_t& dstParms )
{
	bool supportsBlit = true;

	// Check source format properties
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( src->info.fmt ), &formatProperties );

		if ( ( src->info.tiling == IMAGE_TILING_MORTON ) && ( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == false ) {
			supportsBlit = false;
		}
		if ( ( src->info.tiling == IMAGE_TILING_LINEAR ) && ( formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == false ) {
			supportsBlit = false;
		}
	}

	// Check destination format properties
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( dst->info.fmt ), &formatProperties );

		if ( ( dst->info.tiling == IMAGE_TILING_MORTON ) && ( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == false ) {
			supportsBlit = false;
		}
		if ( ( dst->info.tiling == IMAGE_TILING_LINEAR ) && ( formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == false ) {
			supportsBlit = false;
		}
	}

	const VkImageAspectFlagBits srcAspect = vk_GetColorAspectFlags( src->info.fmt );
	const VkImageAspectFlagBits dstAspect = vk_GetColorAspectFlags( dst->info.fmt );

	VkImageMemoryBarrier srcBarrier{ };
	srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	srcBarrier.image = src->gpuImage->GetVkImage( context.bufferId );
	srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.subresourceRange.aspectMask = srcAspect;
	srcBarrier.subresourceRange.baseArrayLayer = srcParms.baseArray;
	srcBarrier.subresourceRange.layerCount = srcParms.arrayCount;
	srcBarrier.subresourceRange.baseMipLevel = srcParms.baseMip;
	srcBarrier.subresourceRange.levelCount = srcParms.mipLevels;

	VkImageMemoryBarrier dstBarrier{ };
	dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	dstBarrier.image = dst->gpuImage->GetVkImage( context.bufferId );
	dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.subresourceRange.aspectMask = dstAspect;
	dstBarrier.subresourceRange.baseArrayLayer = dstParms.baseArray;
	dstBarrier.subresourceRange.layerCount = dstParms.arrayCount;
	dstBarrier.subresourceRange.baseMipLevel = dstParms.baseMip;
	dstBarrier.subresourceRange.levelCount = dstParms.mipLevels;

	// Transition source and destination to transfer layouts
	{
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dstBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		VkImageMemoryBarrier preCopy[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 2, preCopy );
	}

	// Perform blit/copy
	if( supportsBlit )
	{
		// Add multiple regions for each mip-level
		assert( ( srcParms.mipLevels == 1 ) && ( dstParms.mipLevels == 1 ) );

		VkImageBlit blit{ };
		blit.srcOffsets[ 0 ] = { srcParms.x, srcParms.y, srcParms.z };
		blit.srcOffsets[ 1 ] = { srcParms.width, srcParms.height, srcParms.depth };
		blit.srcSubresource.aspectMask = srcAspect;
		blit.srcSubresource.mipLevel = srcParms.baseMip;
		blit.srcSubresource.baseArrayLayer = srcParms.baseArray;
		blit.srcSubresource.layerCount = srcParms.arrayCount;
		blit.dstOffsets[ 0 ] = { dstParms.x, dstParms.y, dstParms.z };
		blit.dstOffsets[ 1 ] = { dstParms.width, dstParms.height, dstParms.depth };
		blit.dstSubresource.aspectMask = dstAspect;
		blit.dstSubresource.baseArrayLayer = dstParms.baseArray;
		blit.dstSubresource.layerCount = dstParms.arrayCount;
		blit.dstSubresource.mipLevel = dstParms.baseMip;

		vkCmdBlitImage( cmdBuffer,
						src->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						dst->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&blit,
						VK_FILTER_LINEAR );
	}
	else
	{
		// Add multiple regions for each mip-level
		assert( ( srcParms.mipLevels == 1 ) && ( dstParms.mipLevels == 1 ) );

		VkImageCopy imageCopyRegion{};
		imageCopyRegion.srcSubresource.aspectMask = srcAspect;
		imageCopyRegion.srcSubresource.layerCount = srcParms.arrayCount;
		imageCopyRegion.srcSubresource.baseArrayLayer = srcParms.baseArray;
		imageCopyRegion.srcSubresource.mipLevel = srcParms.baseMip;
		imageCopyRegion.dstSubresource.aspectMask = dstAspect;
		imageCopyRegion.dstSubresource.layerCount = dstParms.arrayCount;
		imageCopyRegion.dstSubresource.baseArrayLayer = dstParms.baseArray;
		imageCopyRegion.dstSubresource.mipLevel = dstParms.baseMip;
		imageCopyRegion.extent.width = srcParms.width;
		imageCopyRegion.extent.height = srcParms.height;
		imageCopyRegion.extent.depth = srcParms.depth;

		// Issue the copy command
		vkCmdCopyImage(
			cmdBuffer,
			src->gpuImage->GetVkImage( context.bufferId ),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dst->gpuImage->GetVkImage( context.bufferId ),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageCopyRegion );
	}

	// Transition source and destination back to shader read
	{
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkImageMemoryBarrier postCopy[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, postCopy );
	}
}


void vk_CopyImage( VkCommandBuffer cmdBuffer, const Image& src, Image& dst )
{
	copyImageParms_t srcCopy{};
	srcCopy.baseArray = 0;
	srcCopy.arrayCount = src.subResourceView.arrayCount;
	srcCopy.baseMip = 0;
	srcCopy.mipLevels = src.subResourceView.mipLevels;
	srcCopy.x = 0;
	srcCopy.y = 0;
	srcCopy.z = 0;
	srcCopy.width = src.info.width;
	srcCopy.height = src.info.height;
	srcCopy.depth = 1;

	copyImageParms_t dstCopy{};
	dstCopy.baseArray = 0;
	dstCopy.arrayCount = dst.subResourceView.arrayCount;
	dstCopy.baseMip = 0;
	dstCopy.mipLevels = dst.subResourceView.mipLevels;
	dstCopy.x = 0;
	dstCopy.y = 0;
	dstCopy.z = 0;
	dstCopy.width = dst.info.width;
	dstCopy.height = dst.info.height;
	dstCopy.depth = 1;

	vk_CopyImage( cmdBuffer, &src, srcCopy, &dst, dstCopy );
}


void vk_CopyImage( VkCommandBuffer cmdBuffer, const ImageView& src, ImageView& dst )
{
	copyImageParms_t srcCopy{};
	srcCopy.baseArray = src.subResourceView.baseArray;
	srcCopy.arrayCount = src.subResourceView.arrayCount;
	srcCopy.baseMip = src.subResourceView.baseMip;
	srcCopy.mipLevels = src.subResourceView.mipLevels;
	srcCopy.x = 0;
	srcCopy.y = 0;
	srcCopy.z = 0;
	srcCopy.width = src.info.width;
	srcCopy.height = src.info.height;
	srcCopy.depth = 1;

	copyImageParms_t dstCopy{};
	dstCopy.baseArray = dst.subResourceView.baseArray;
	dstCopy.arrayCount = dst.subResourceView.arrayCount;
	dstCopy.baseMip = dst.subResourceView.baseMip;
	dstCopy.mipLevels = dst.subResourceView.mipLevels;
	dstCopy.x = 0;
	dstCopy.y = 0;
	dstCopy.z = 0;
	dstCopy.width = dst.info.width;
	dstCopy.height = dst.info.height;
	dstCopy.depth = 1;

	vk_CopyImage( cmdBuffer, &src, srcCopy, &dst, dstCopy );
}


void vk_ResolveImage( VkCommandBuffer cmdBuffer, const resolveImageInfo_t& info )
{
	const VkImageAspectFlagBits aspect  = vk_GetColorAspectFlags( info.src->info.fmt );
	const bool                  isDepth = ( aspect & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;

	// Depth and color images live in different layouts and are owned by different pipeline stages
	const VkImageLayout attachmentLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	const VkImageLayout readLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	const VkPipelineStageFlags attachmentStage = isDepth ? ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT )
	                                                        : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const VkAccessFlags attachmentWrite = isDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	const VkAccessFlags attachmentRW = isDepth ? ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT )
												: ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );

	// Transition source to TRANSFER_SRC_OPTIMAL
	VkImageMemoryBarrier srcBarrier{ };
	srcBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	srcBarrier.image                           = info.src->gpuImage->GetVkImage( context.bufferId );
	srcBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
	srcBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.subresourceRange.aspectMask     = aspect;
	srcBarrier.subresourceRange.baseMipLevel   = info.baseMip;
	srcBarrier.subresourceRange.levelCount     = 1;
	srcBarrier.subresourceRange.baseArrayLayer = info.baseArray;
	srcBarrier.subresourceRange.layerCount     = info.arrayCount;

	VkPipelineStageFlags srcStage;
	if ( info.transitionSourceFromWrite )
	{
		srcBarrier.oldLayout     = attachmentLayout;
		srcBarrier.srcAccessMask = attachmentWrite;
		srcStage                 = attachmentStage;
	}
	else
	{
		srcBarrier.oldLayout     = readLayout;
		srcBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage                 = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	// Transition destination to TRANSFER_DST_OPTIMAL
	VkImageMemoryBarrier dstBarrier{ };
	dstBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	dstBarrier.image                           = info.dst->gpuImage->GetVkImage( context.bufferId );
	dstBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
	dstBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.srcAccessMask                   = 0;
	dstBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.subresourceRange.aspectMask     = aspect;
	dstBarrier.subresourceRange.baseMipLevel   = info.baseMip;
	dstBarrier.subresourceRange.levelCount     = 1;
	dstBarrier.subresourceRange.baseArrayLayer = info.baseArray;
	dstBarrier.subresourceRange.layerCount     = info.arrayCount;

	VkImageMemoryBarrier preBlit[ 2 ] = { srcBarrier, dstBarrier };
	vkCmdPipelineBarrier( cmdBuffer,
		srcStage,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 2, preBlit );

	VkImageResolve region{ };
	region.srcSubresource.aspectMask     = aspect;
	region.srcSubresource.mipLevel       = info.baseMip;
	region.srcSubresource.baseArrayLayer = info.baseArray;
	region.srcSubresource.layerCount     = info.arrayCount;
	region.srcOffset                     = { 0, 0, 0 };
	region.dstSubresource.aspectMask     = aspect;
	region.dstSubresource.mipLevel       = info.baseMip;
	region.dstSubresource.baseArrayLayer = info.baseArray;
	region.dstSubresource.layerCount     = info.arrayCount;
	region.dstOffset                     = { 0, 0, 0 };
	region.extent                        = { info.src->info.width, info.src->info.height, 1 };

	vkCmdResolveImage(
		cmdBuffer,
		info.src->gpuImage->GetVkImage( context.bufferId ), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		info.dst->gpuImage->GetVkImage( context.bufferId ), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region );

	// Destination always goes to SHADER_READ_ONLY_OPTIMAL — resolved images are sampled, not written
	dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	if ( info.writeSourceAfterResolve )
	{
		// Restore source to its attachment layout so the next render pass can write to it
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout     = attachmentLayout;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = attachmentRW;

		VkImageMemoryBarrier postResolve[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			attachmentStage | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, postResolve );
	}
	else
	{
		// Restore source to its read layout for subsequent shader sampling
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout     = readLayout;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkImageMemoryBarrier postResolve[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, postResolve );
	}
}


void vk_UploadImageData( VkCommandBuffer cmdBuffer, Image* image, const copyImageParms_t& copyParms, GpuBuffer& buffer )
{
	std::vector<VkBufferImageCopy> regions;
	regions.resize( copyParms.mipLevels );

	const uint64_t bufferSize = buffer.GetSize();
	// bufferOffset must be a multiple of the texel block size (up to 16 bytes for BC/RGBA32F)
	// https://docs.vulkan.org/spec/latest/chapters/copies.html#VUID-vkCmdCopyBufferToImage-dstImage-07975
	const uint32_t bpp = GetBppForFormat( image->info.fmt );
	const uint64_t copyAlignment = Max( context.deviceProperties.limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>( bpp ) );
	const uint64_t alignmentOffset = buffer.GetAlignedSize( bufferSize, copyAlignment );
	buffer.SetPos( alignmentOffset );

	for( uint32_t mip = 0; mip < copyParms.mipLevels; ++mip )
	{
		VkBufferImageCopy& region = regions[ mip ];
		memset( &region, 0, sizeof( VkBufferImageCopy ) );

		region.bufferOffset = buffer.GetSize();

		// Just assume continuous layers for now
		for ( uint32_t layer = 0; layer < image->info.layers; ++layer )
		{
			const slice_t imageBuffer = image->cpuImage->GetSlice( layer, mip );
			buffer.CopyData( imageBuffer.ptr, imageBuffer.size );
		}

		region.imageSubresource.aspectMask = vk_GetColorAspectFlags( image->info.fmt );
		region.imageSubresource.mipLevel = copyParms.baseMip + mip;
		region.imageSubresource.baseArrayLayer = copyParms.baseArray;
		region.imageSubresource.layerCount = copyParms.arrayCount;

		uint32_t mipWidth, mipHeight;
		MipDimensions( mip, copyParms.width, copyParms.height, &mipWidth, &mipHeight );

		const int32_t x = static_cast<int32_t>( ( copyParms.x / (float)copyParms.width ) * mipWidth );
		const int32_t y = static_cast<int32_t>( ( copyParms.y / (float)copyParms.height ) * mipHeight );

		region.imageOffset = { x, y, 0 };
		region.imageExtent = {
			static_cast<uint32_t>( mipWidth ),
			static_cast<uint32_t>( mipHeight ),
			static_cast<uint32_t>( 1.0f ),
		};
	}

	vkCmdCopyBufferToImage( cmdBuffer,
							buffer.GetVkObject(),
							image->gpuImage->GetVkImage( context.bufferId ),
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							static_cast<uint32_t>( regions.size() ),
							regions.data()
	);
}


imageSamples_t vk_MaxImageSamples()
{
	if( ForceDisableMSAA ) {
		return IMAGE_SMP_1;
	}

	imageSamples_t samples = IMAGE_SMP_1;

	VkSampleCountFlags frameBufferCount = context.deviceProperties.limits.framebufferColorSampleCounts;
	VkSampleCountFlags depthBufferCount = context.deviceProperties.limits.framebufferDepthSampleCounts;
	VkSampleCountFlags counts = ( frameBufferCount & depthBufferCount );

	if ( counts & VK_SAMPLE_COUNT_64_BIT ) { samples = IMAGE_SMP_64; }
	else if ( counts & VK_SAMPLE_COUNT_32_BIT ) { samples = IMAGE_SMP_32; }
	else if ( counts & VK_SAMPLE_COUNT_16_BIT ) { samples = IMAGE_SMP_16; }
	else if ( counts & VK_SAMPLE_COUNT_8_BIT ) { samples = IMAGE_SMP_8; }
	else if ( counts & VK_SAMPLE_COUNT_4_BIT ) { samples = IMAGE_SMP_4; }
	else if ( counts & VK_SAMPLE_COUNT_2_BIT ) { samples = IMAGE_SMP_2; }

	return samples;
}


VkShaderModule vk_CreateShaderModule( const std::vector<char>& code, const char* debugName )
{
	VkShaderModuleCreateInfo createInfo{ };
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>( code.data() );

	VkShaderModule shaderModule;
	VK_CHECK_RESULT( vkCreateShaderModule( context.device, &createInfo, nullptr, &shaderModule ) );

	vk_SetObjectName( (uint64_t)shaderModule, VK_OBJECT_TYPE_SHADER_MODULE, vk_BuildObjectName( "Shader", debugName ).c_str() );

	return shaderModule;
}


VkResult vk_CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}


std::string vk_BuildObjectName( const char* typeName, const char* baseName, int32_t bufferId )
{
	std::stringstream dbgNameSS;
	dbgNameSS.clear();

	dbgNameSS << typeName << ": " << baseName;
	if( bufferId >= 0 ) {
		dbgNameSS << "(" << bufferId << ")";
	}
	return dbgNameSS.str();
}


void vk_SetObjectName( const uint64_t handle, VkObjectType objectType, const char* name )
{
	if ( context.fnCmdSetDebugUtilsObjectName == nullptr ) {
		return;
	}

	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = objectType;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = name;

	context.fnCmdSetDebugUtilsObjectName( context.device, &nameInfo );
}


void vk_MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerObjectTagInfoEXT tagInfo = {};
		tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
		tagInfo.objectType = objectType;
		tagInfo.object = object;
		tagInfo.tagName = name;
		tagInfo.tagSize = tagSize;
		tagInfo.pTag = tag;
		context.fnDebugMarkerSetObjectTag( context.device, &tagInfo );
	}
}


void vk_MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = objectType;
		nameInfo.object = object;
		nameInfo.pObjectName = name;
		context.fnDebugMarkerSetObjectName( context.device, &nameInfo );
	}
}


static VKAPI_ATTR VkBool32 VKAPI_CALL vk_DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData )
{
	// DXC culls input/outputs and Vulkans warns on unused attributes
	// The shaders are set-up with universal bindings so unused attributes are expected
	// These are really informational warnings so can be suppressed
	static const int32_t suppressedIds[] =
	{
		(int32_t)0x609A13B,		// UNASSIGNED-CoreValidation-Shader-OutputNotConsumed
		(int32_t)0xC81AD50E,	// UNASSIGNED-CoreValidation-Shader-InputNotProduced
	};

	for ( int32_t id : suppressedIds )
	{
		if ( pCallbackData->messageIdNumber == id ) {
			return VK_FALSE;
		}
	}

	std::cerr << "[Vulkan Validation - ";
	if( ( messageType &= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) != 0 ) {
		std::cerr << "Error";
	} else if ( ( messageType &= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ) != 0 ) {
		std::cerr << "Performance";
	}
	std::cerr << "]\t";
	std::cerr << pCallbackData->pMessage << "\n" << std::endl;
	return VK_FALSE;
}


void vk_PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
{
	createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = 0;
	if ( ValidateVerbose ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	}

	if ( ValidateWarnings ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	if ( ValidateErrors ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	VkDebugUtilsMessageTypeFlagsEXT messageFlags = 0;
	messageFlags |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
	messageFlags |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	messageFlags |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.messageType = messageFlags;
	createInfo.pfnUserCallback = vk_DebugCallback;
}


bool vk_CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

	std::vector<VkLayerProperties> availableLayers( layerCount );
	vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

	for ( const char* layerName : context.validationLayers ) {
		bool layerFound = false;
		for ( const auto& layerProperties : availableLayers ) {
			if ( strcmp( layerName, layerProperties.layerName ) == 0 ) {
				layerFound = true;
				break;
			}
		}
		if ( !layerFound ) {
			return false;
		}
	}
	return true;
}


std::vector<const char*> vk_GetRequiredExtensions()
{
#ifdef USE_GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

	std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );
#else
	std::vector<const char*> extensions;
#endif

	if ( EnableValidationLayers )
	{
		extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		extensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
	}

	return extensions;
}


void DeviceContext::Create( Window& window )
{
	// Create Instance
	{
		if ( EnableValidationLayers && !vk_CheckValidationLayerSupport() )
		{
			throw std::runtime_error( "validation layers requested, but not available!" );
		}

		VkApplicationInfo appInfo{ };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Extensa";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
		std::vector<VkExtensionProperties> extensionProperties( extensionCount );
		vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensionProperties.data() );

		std::cout << "Available extensions:\n";

		for ( const auto& extension : extensionProperties ) {
			std::cout << '\t' << extension.extensionName << '\n';
		}

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if ( EnableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();

			vk_PopulateDebugMessengerCreateInfo( debugCreateInfo );
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		auto extensions = vk_GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
		createInfo.ppEnabledExtensionNames = extensions.data();

		VK_CHECK_RESULT( vkCreateInstance( &createInfo, nullptr, &instance ) );

		vk_SetObjectName( (uint64_t)instance, VK_OBJECT_TYPE_INSTANCE, "VulkanInstance" );
	}

	// Debug Messenger
	{
		if ( !EnableValidationLayers ) {
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		vk_PopulateDebugMessengerCreateInfo( createInfo );

		VK_CHECK_RESULT( vk_CreateDebugUtilsMessengerEXT( instance, &createInfo, nullptr, &debugMessenger ) );
	}

	// Window Surface
	{
#ifdef USE_GLFW
		window.CreateGlfwSurface( context.instance );
#endif
	}

	isRendeDocAttached = ::IsRenderDocAttached();

	// Pick physical device
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr );

		if ( deviceCount == 0 ) {
			throw std::runtime_error( "Failed to find GPUs with Vulkan support!" );
		}

		std::vector<VkPhysicalDevice> devices( deviceCount );
		vkEnumeratePhysicalDevices( instance, &deviceCount, devices.data() );

		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

		for ( const auto& device : devices )
		{
			if ( vk_IsDeviceSuitable( device, window.vk_surface, deviceExtensions ) )
			{
				vkGetPhysicalDeviceProperties( device, &deviceProperties );
				vkGetPhysicalDeviceFeatures2( device, &deviceFeatures );
				physicalDevice = device;
				break;
			}
		}

		if ( physicalDevice == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to find a suitable GPU!" );
		}
		vk_SetObjectName( (uint64_t)physicalDevice, VK_OBJECT_TYPE_DEVICE, "VulkanPhysicalDevice" );
	}

	// Create logical device
	{
		QueueFamilyIndices indices = vk_FindQueueFamilies( physicalDevice, window.vk_surface );
		queueFamilyIndices[ QUEUE_GRAPHICS ] = indices.graphicsFamily.value();
		queueFamilyIndices[ QUEUE_PRESENT ] = indices.presentFamily.value();
		queueFamilyIndices[ QUEUE_COMPUTE ] = indices.computeFamily.value();

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value() };

		float queuePriority = 1.0f;
		for ( uint32_t queueFamily : uniqueQueueFamilies )
		{
			VkDeviceQueueCreateInfo queueCreateInfo{ };
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		VkDeviceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>( queueCreateInfos.size() );
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		deviceFeatures.features.samplerAnisotropy = VK_TRUE;
		deviceFeatures.features.fillModeNonSolid = VK_TRUE;
		deviceFeatures.features.sampleRateShading = VK_TRUE;
		deviceFeatures.features.pipelineStatisticsQuery = VK_TRUE;
		deviceFeatures.features.vertexPipelineStoresAndAtomics = VK_TRUE;
		deviceFeatures.features.fragmentStoresAndAtomics = VK_TRUE;
#ifdef USE_VULKAN_RTX
		deviceFeatures.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
	
		enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;

		enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
		enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;

		enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
		enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

		deviceFeatures.pNext = &accelerationStructureFeatures;
		vkGetPhysicalDeviceFeatures2( physicalDevice, &deviceFeatures );
#endif

		createInfo.pEnabledFeatures = &deviceFeatures.features;

		std::vector<const char*> enabledExtensions;
		for ( auto ext : deviceExtensions )
		{
			enabledExtensions.push_back( ext );
		}
		std::vector<const char*> captureExtensions = { VK_EXT_DEBUG_MARKER_EXTENSION_NAME };
		if ( vk_IsDeviceSuitable( physicalDevice, window.vk_surface, captureExtensions ) ) {
			for ( const char* ext : captureExtensions ) {
				enabledExtensions.push_back( ext );
			}
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>( enabledExtensions.size() );
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();
		if ( EnableValidationLayers )
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>( validationLayers.size() );
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature = {};
		dynamicRenderingFeature.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		dynamicRenderingFeature.dynamicRendering = VK_TRUE;
		dynamicRenderingFeature.pNext            = nullptr;

		VkPhysicalDeviceDescriptorIndexingFeatures descIndexing;
		memset( &descIndexing, 0, sizeof( VkPhysicalDeviceDescriptorIndexingFeatures ) );
		descIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		descIndexing.pNext = &dynamicRenderingFeature;
		descIndexing.runtimeDescriptorArray = true;
		descIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
#ifdef USE_VULKAN_RTX
		enabledBufferDeviceAddresFeatures.pNext = &descIndexing;
		createInfo.pNext = &enabledAccelerationStructureFeatures;
#else
		createInfo.pNext = &descIndexing;
#endif

		VK_CHECK_RESULT( vkCreateDevice( physicalDevice, &createInfo, nullptr, &device ) );
		vk_SetObjectName( (uint64_t)device, VK_OBJECT_TYPE_DEVICE, "VulkanLogicalDevice" );

		vkGetDeviceQueue( device, indices.graphicsFamily.value(), 0, &gfxContext );
		vkGetDeviceQueue( device, indices.presentFamily.value(), 0, &presentQueue );
		vkGetDeviceQueue( device, indices.computeFamily.value(), 0, &computeContext );
	}

	// Debug Markers
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &extensionCount, nullptr );

		std::vector<VkExtensionProperties> availableExtensions( extensionCount );
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &extensionCount, availableExtensions.data() );

		bool found = false;
		for ( const auto& extension : availableExtensions ) {
			if ( strcmp( extension.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				found = true;
				break;
			}
		}

#ifdef USE_VULKAN_RTX
		// Get ray tracing pipeline properties, which will be used later on in the sample
		//rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		//VkPhysicalDeviceProperties2 deviceProperties2{};
		//deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		//deviceProperties2.pNext = &rayTracingPipelineProperties;
		//vkGetPhysicalDeviceProperties2( physicalDevice, &deviceProperties2 );

		vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>( vkGetDeviceProcAddr( device, "vkGetBufferDeviceAddressKHR" ) );
		vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>( vkGetDeviceProcAddr( device, "vkCmdBuildAccelerationStructuresKHR" ) );
		vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>( vkGetDeviceProcAddr( device, "vkBuildAccelerationStructuresKHR" ) );
		vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>( vkGetDeviceProcAddr( device, "vkCreateAccelerationStructureKHR" ) );
		vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>( vkGetDeviceProcAddr( device, "vkDestroyAccelerationStructureKHR" ) );
		vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>( vkGetDeviceProcAddr( device, "vkGetAccelerationStructureBuildSizesKHR" ) );
		vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>( vkGetDeviceProcAddr( device, "vkGetAccelerationStructureDeviceAddressKHR" ) );
		vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>( vkGetDeviceProcAddr( device, "vkCmdTraceRaysKHR" ) );
		vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>( vkGetDeviceProcAddr( device, "vkGetRayTracingShaderGroupHandlesKHR" ) );
		vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>( vkGetDeviceProcAddr( device, "vkCreateRayTracingPipelinesKHR" ) );
#endif

		if ( found )
		{
			std::cout << "Enabling debug markers." << std::endl;

			fnDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr( device, "vkDebugMarkerSetObjectTagEXT" );
			fnDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr( device, "vkDebugMarkerSetObjectNameEXT" );
			fnCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerBeginEXT" );
			fnCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerEndEXT" );
			fnCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerInsertEXT" );

			debugMarkersEnabled = true;
			debugMarkersEnabled = debugMarkersEnabled && ( fnDebugMarkerSetObjectTag != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnDebugMarkerSetObjectName != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerBegin != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerEnd != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerInsert != VK_NULL_HANDLE );
		}
		else {
			std::cout << "Debug markers \"" << VK_EXT_DEBUG_MARKER_EXTENSION_NAME << "\" disabled." << std::endl;
		}
	}

	// Debug
	{
		fnCmdSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr( device, "vkSetDebugUtilsObjectNameEXT" );
	}

	// Descriptor Pool
	{
		const uint32_t subPoolCount = 6;

		VkDescriptorPoolSize poolSizes[ subPoolCount ];
		poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[ 0 ].descriptorCount = DescriptorPoolMaxUniformBuffers;
		poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[ 1 ].descriptorCount = DescriptorPoolMaxStorageBuffers;
		poolSizes[ 2 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[ 2 ].descriptorCount = DescriptorPoolMaxComboImages;
		poolSizes[ 3 ].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSizes[ 3 ].descriptorCount = DescriptorPoolMaxImages;
		poolSizes[ 4 ].type = VK_DESCRIPTOR_TYPE_SAMPLER;
		poolSizes[ 4 ].descriptorCount = DescriptorPoolMaxSamplers;
		poolSizes[ 5 ].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[ 5 ].descriptorCount = DescriptorPoolMaxStorageImages;

		VkDescriptorPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = subPoolCount;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = DescriptorPoolMaxSets;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		VK_CHECK_RESULT( vkCreateDescriptorPool( device, &poolInfo, nullptr, &descriptorPool ) );

		vk_SetObjectName( (uint64_t)descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "DescriptorPool (Uniform | Storage | ImageSampler | Image)" );
	}

	bufferId = 0;
}


void DeviceContext::Destroy( Window& window )
{
	vkDestroyQueryPool( device, statQueryPool, nullptr );
	vkDestroyQueryPool( device, timestampQueryPool, nullptr );
	vkDestroyQueryPool( device, occlusionQueryPool, nullptr );

	vkDestroyDescriptorPool( device, descriptorPool, nullptr );

	vkDestroyDevice( device, nullptr );

	if ( EnableValidationLayers )
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
		if ( func != nullptr ) {
			func( instance, debugMessenger, nullptr );
		}
	}

#ifdef USE_GLFW
	window.DestroyGlfwSurface( context.instance );
#endif
	
	vkDestroyInstance( instance, nullptr );
}
