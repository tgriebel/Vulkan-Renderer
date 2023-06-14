#include <gfxcore/asset_types/texture.h>
#include "FrameState.h"
#include "deviceContext.h"
#include "rhi.h"
#include "../render_core/swapChain.h"

extern SwapChain g_swapChain;

VkRenderPass vk_CreateRenderPass( const vk_RenderPassBits_t& passState )
{
	VkRenderPass pass = VK_NULL_HANDLE;

	VkAttachmentReference colorAttachmentRef[ 3 ] = { };
	VkAttachmentReference dsAttachmentRef{ };

	uint32_t count = 0;
	uint32_t colorCount = 0;

	colorAttachmentRef[ 0 ].attachment = VK_ATTACHMENT_UNUSED;
	colorAttachmentRef[ 1 ].attachment = VK_ATTACHMENT_UNUSED;
	colorAttachmentRef[ 2 ].attachment = VK_ATTACHMENT_UNUSED;
	dsAttachmentRef.attachment = VK_ATTACHMENT_UNUSED;

	VkAttachmentDescription attachments[ 5 ] = {};

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR0 ) != 0 )
	{
		if ( passState.semantic.colorTrans0.flags.presentAfter ) {
			attachments[ count ].format = vk_GetTextureFormat( g_swapChain.GetBackBufferFormat() );
		}
		else {
			attachments[ count ].format = vk_GetTextureFormat( passState.semantic.colorAttach0.fmt );
		}
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.colorAttach0.samples );
		attachments[ count ].loadOp = passState.semantic.colorTrans0.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].storeOp = passState.semantic.colorTrans0.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.colorTrans0.flags.presentAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
		else if ( passState.semantic.colorTrans0.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR1 ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.colorAttach1.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.colorAttach1.samples );
		attachments[ count ].loadOp = passState.semantic.colorTrans1.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.colorTrans1.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.colorTrans1.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR2 ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.colorAttach2.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.colorAttach2.samples );
		attachments[ count ].loadOp = passState.semantic.colorTrans2.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.colorTrans2.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.colorTrans1.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	colorCount = count;

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_DEPTH ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.depthAttach.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.depthAttach.samples );
		attachments[ count ].loadOp = passState.semantic.depthTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.depthTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.depthTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.depthTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.depthTrans.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		dsAttachmentRef.attachment = count;
		dsAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_STENCIL ) != 0 )
	{
		assert( ( passState.semantic.attachmentMask& RENDER_PASS_MASK_DEPTH ) != 0 );

		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.stencilAttach.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.stencilAttach.samples );
		attachments[ count ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.stencilTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.stencilTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.stencilTrans.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		++count;
	}

	VkSubpassDescription subpass{ };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorCount;
	subpass.pColorAttachments = colorAttachmentRef;
	subpass.pDepthStencilAttachment = &dsAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{ };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = count;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if ( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &pass ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create render pass!" );
	}
	return pass;
}


void FrameBuffer::Create( const frameBufferCreateInfo_t& createInfo )
{
	renderPassTransitionFlags_t perms[ PassPermCount ];
	for ( uint32_t i = 0; i < PassPermCount; ++i ) {
		perms[ i ].bits = i;
	}

	const uint32_t frameCount = 1;
	const bool canPresent = ( createInfo.color0[ 0 ] != nullptr ) && ( createInfo.color0[ 0 ]->info.fmt == g_swapChain.GetBackBufferFormat() );

	// Validation
	for ( uint32_t frameIx = 1; frameIx < createInfo.bufferCount; ++frameIx )
	{
		bool differentImages = true;
		differentImages = differentImages && ( createInfo.color0[ frameIx - 1 ] != createInfo.color0[ frameIx ] );
		differentImages = differentImages && ( createInfo.color1[ frameIx - 1 ] != createInfo.color1[ frameIx ] );
		differentImages = differentImages && ( createInfo.color2[ frameIx - 1 ] != createInfo.color2[ frameIx ] );
		differentImages = differentImages && ( createInfo.depth[ frameIx - 1 ] != createInfo.depth[ frameIx ] );
		differentImages = differentImages && ( createInfo.stencil[ frameIx - 1 ] != createInfo.stencil[ frameIx ] );

		if ( differentImages == false ) {
			throw std::runtime_error( "Framebuffer buffer layers refer to same images." );
		}

		bool infoMatches = true;
		infoMatches = infoMatches && ( createInfo.color0[ frameIx - 1 ]->info == createInfo.color0[ frameIx ]->info );
		infoMatches = infoMatches && ( createInfo.color1[ frameIx - 1 ]->info == createInfo.color1[ frameIx ]->info );
		infoMatches = infoMatches && ( createInfo.color2[ frameIx - 1 ]->info == createInfo.color2[ frameIx ]->info );
		infoMatches = infoMatches && ( createInfo.depth[ frameIx - 1 ]->info == createInfo.depth[ frameIx ]->info );
		infoMatches = infoMatches && ( createInfo.stencil[ frameIx - 1 ]->info == createInfo.stencil[ frameIx ]->info );

		if ( infoMatches == false ) {
			throw std::runtime_error( "Framebuffer images have different properties." );
		}
	}

	colorCount += ( createInfo.color0[ 0 ] != nullptr ) ? 1 : 0;
	colorCount += ( createInfo.color1[ 0 ] != nullptr ) ? 1 : 0;
	colorCount += ( createInfo.color2[ 0 ] != nullptr ) ? 1 : 0;
	dsCount += ( createInfo.depth[ 0 ] != nullptr ) ? 1 : 0;
	dsCount += ( createInfo.stencil[ 0 ] != nullptr ) ? 1 : 0;

	attachmentCount = colorCount + dsCount;

	for ( uint32_t permIx = 0; permIx < PassPermCount; ++permIx ) {
		for ( uint32_t frameIx = 0; frameIx < frameCount; ++frameIx ) {
			buffers[ frameIx ][ permIx ] = VK_NULL_HANDLE;
		}
		renderPasses[ permIx ] = VK_NULL_HANDLE;
	}

	for( uint32_t permIx = 0; permIx < PassPermCount; ++permIx )
	{
		const renderPassTransitionFlags_t& state = perms[ permIx ];

		if( ( canPresent == false ) && state.flags.presentAfter ) {
			continue;
		}

		// Can specify clear options, etc. Assigned to cached render pass that matches
		VkImageView attachments[ 5 ] = {};

		vk_RenderPassBits_t passBits = {};

		uint32_t currentAttachment = 0;
		if( createInfo.color0[ 0 ] != nullptr )
		{
			passBits.semantic.colorAttach0.samples = createInfo.color0[ 0 ]->info.subsamples;
			passBits.semantic.colorAttach0.fmt = createInfo.color0[ 0 ]->info.fmt;
			passBits.semantic.colorTrans0 = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR0;

			attachments[ currentAttachment++ ] = createInfo.color0[ 0 ]->gpuImage->GetVkImageView();
		}

		if ( createInfo.color1[ 0 ] != nullptr )
		{
			passBits.semantic.colorAttach1.samples = createInfo.color1[ 0 ]->info.subsamples;
			passBits.semantic.colorAttach1.fmt = createInfo.color1[ 0 ]->info.fmt;
			passBits.semantic.colorTrans1 = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR1;

			attachments[ currentAttachment++ ] = createInfo.color1[ 0 ]->gpuImage->GetVkImageView();
		}

		if ( createInfo.color2[ 0 ] != nullptr )
		{
			passBits.semantic.colorAttach2.samples = createInfo.color2[ 0 ]->info.subsamples;
			passBits.semantic.colorAttach2.fmt = createInfo.color2[ 0 ]->info.fmt;
			passBits.semantic.colorTrans2 = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR2;

			attachments[ currentAttachment++ ] = createInfo.color2[ 0 ]->gpuImage->GetVkImageView();
		}

		if ( createInfo.depth[ 0 ] != nullptr )
		{
			passBits.semantic.depthAttach.samples = createInfo.depth[ 0 ]->info.subsamples;
			passBits.semantic.depthAttach.fmt = createInfo.depth[ 0 ]->info.fmt;
			passBits.semantic.depthTrans = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_DEPTH;

			attachments[ currentAttachment++ ] = createInfo.depth[ 0 ]->gpuImage->GetVkImageView();
		}

		if ( createInfo.stencil[ 0 ] != nullptr )
		{
			passBits.semantic.stencilAttach.samples = createInfo.stencil[ 0 ]->info.subsamples;
			passBits.semantic.stencilAttach.fmt = createInfo.stencil[ 0 ]->info.fmt;
			passBits.semantic.stencilTrans = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_STENCIL;

			attachments[ currentAttachment++ ] = createInfo.stencil[ 0 ]->gpuImage->GetVkImageView();
		}
		assert( currentAttachment == attachmentCount );

		renderPasses[ permIx ] = vk_CreateRenderPass( passBits );
		if ( renderPasses[ permIx ] == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to create framebuffer! Render pass invalid" );
		}

		for ( uint32_t frameIx = 0; frameIx < frameCount; ++frameIx )
		{
			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPasses[ permIx ];
			framebufferInfo.attachmentCount = attachmentCount;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = createInfo.width;
			framebufferInfo.height = createInfo.height;
			framebufferInfo.layers = 1;

			if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &buffers[ frameIx ][ permIx ] ) != VK_SUCCESS ) {
				throw std::runtime_error( "Failed to create framebuffer!" );
			}
		}
	}

	color0[ 0 ] = createInfo.color0[ 0 ];
	color1[ 0 ] = createInfo.color1[ 0 ];
	color2[ 0 ] = createInfo.color2[ 0 ];
	depth[ 0 ] = createInfo.depth[ 0 ];
	stencil[ 0 ] = createInfo.stencil[ 0 ];
	width = createInfo.width;
	height = createInfo.height;
}


void FrameBuffer::Destroy()
{
	assert( context.device != VK_NULL_HANDLE );
	if ( context.device != VK_NULL_HANDLE )
	{
		for ( uint32_t i = 0; i < PassPermCount; ++i )
		{
			if ( buffers != VK_NULL_HANDLE ) {
				vkDestroyFramebuffer( context.device, buffers[ 0 ][ i ], nullptr );
			}
			if ( renderPasses != VK_NULL_HANDLE ) {
				vkDestroyRenderPass( context.device, renderPasses[ i ], nullptr );
			}
		}
	}
}