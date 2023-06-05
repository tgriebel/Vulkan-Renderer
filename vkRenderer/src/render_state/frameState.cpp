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

	const bool canPresent = ( createInfo.color0 != nullptr ) && ( createInfo.color0->info.fmt == g_swapChain.GetBackBufferFormat() );

	for( uint32_t i = 0; i < PassPermCount; ++i )
	{
		const renderPassTransitionFlags_t& state = perms[ i ];

		if( ( canPresent == false ) && state.flags.presentAfter )
		{
			buffers[ 0 ][ i ] = VK_NULL_HANDLE;
			renderPasses[ i ] = VK_NULL_HANDLE;
			continue;
		}

		// Can specify clear options, etc. Assigned to cached render pass that matches
		attachmentCount = 0;
		VkImageView attachments[ 5 ] = {};

		uint32_t colorCount = 0;
		uint32_t dsCount = 0;

		vk_RenderPassBits_t passBits = {};

		if( createInfo.color0 != nullptr )
		{
			passBits.semantic.colorAttach0.samples = createInfo.color0->info.subsamples;
			passBits.semantic.colorAttach0.fmt = createInfo.color0->info.fmt;
			passBits.semantic.colorTrans0 = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR0;

			attachments[ colorCount ] = createInfo.color0->gpuImage->GetVkImageView();
			++colorCount;
		}

		if ( createInfo.color1 != nullptr )
		{
			passBits.semantic.colorAttach1.samples = createInfo.color1->info.subsamples;
			passBits.semantic.colorAttach1.fmt = createInfo.color1->info.fmt;
			passBits.semantic.colorTrans1 = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR1;

			attachments[ colorCount ] = createInfo.color1->gpuImage->GetVkImageView();
			++colorCount;
		}

		if ( createInfo.color2 != nullptr )
		{
			passBits.semantic.colorAttach2.samples = createInfo.color2->info.subsamples;
			passBits.semantic.colorAttach2.fmt = createInfo.color2->info.fmt;
			passBits.semantic.colorTrans2 = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR2;

			attachments[ colorCount ] = createInfo.color2->gpuImage->GetVkImageView();
			++colorCount;
		}

		if ( createInfo.depth != nullptr )
		{
			passBits.semantic.depthAttach.samples = createInfo.depth->info.subsamples;
			passBits.semantic.depthAttach.fmt = createInfo.depth->info.fmt;
			passBits.semantic.depthTrans = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_DEPTH;

			attachments[ colorCount ] = createInfo.depth->gpuImage->GetVkImageView();
			++dsCount;
		}

		if ( createInfo.stencil != nullptr )
		{
			passBits.semantic.stencilAttach.samples = createInfo.stencil->info.subsamples;
			passBits.semantic.stencilAttach.fmt = createInfo.stencil->info.fmt;
			passBits.semantic.stencilTrans = state;
			passBits.semantic.attachmentMask |= RENDER_PASS_MASK_STENCIL;

			attachments[ colorCount + dsCount ] = createInfo.stencil->gpuImage->GetVkImageView();
			++dsCount;
		}

		attachmentCount = colorCount + dsCount;

		renderPasses[ i ] = vk_CreateRenderPass( passBits );
		if ( renderPasses[ i ] == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to create framebuffer! Render pass invalid" );
		}

		VkFramebufferCreateInfo framebufferInfo{ };
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPasses[ i ];
		framebufferInfo.attachmentCount = attachmentCount;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = createInfo.width;
		framebufferInfo.height = createInfo.height;
		framebufferInfo.layers = 1;

		if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &buffers[ 0 ][ i ] ) != VK_SUCCESS ) {
			throw std::runtime_error( "Failed to create framebuffer!" );
		}
	}

	color0 = createInfo.color0;
	color1 = createInfo.color1;
	color2 = createInfo.color2;
	depth = createInfo.depth;
	stencil = createInfo.stencil;
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