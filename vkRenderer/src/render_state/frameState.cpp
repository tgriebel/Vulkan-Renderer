#include "FrameState.h"
#include "deviceContext.h"
#include "rhi.h"
#include <resource_types/texture.h>

VkRenderPass vk_CreateRenderPass( const vk_RenderPassBits_t& passState )
{
	VkRenderPass pass = VK_NULL_HANDLE;

	VkAttachmentReference colorAttachmentRef[ 3 ] = { };
	VkAttachmentReference depthAttachmentRef{ };
	VkAttachmentReference stencilAttachmentRef{ };

	uint32_t count = 0;
	uint32_t colorCount = 0;

	colorAttachmentRef[ 0 ].attachment = VK_ATTACHMENT_UNUSED;
	colorAttachmentRef[ 1 ].attachment = VK_ATTACHMENT_UNUSED;
	colorAttachmentRef[ 2 ].attachment = VK_ATTACHMENT_UNUSED;
	depthAttachmentRef.attachment = VK_ATTACHMENT_UNUSED;
	stencilAttachmentRef.attachment = VK_ATTACHMENT_UNUSED;

	VkAttachmentDescription attachments[ 5 ] = {};

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR0 ) != 0 )
	{
		if ( passState.semantic.colorAttach0.presentAfter ) {
			attachments[ count ].format = vk_GetTextureFormat( TEXTURE_FMT_BGRA_8 ); // FIXME
		}
		else {
			attachments[ count ].format = vk_GetTextureFormat( passState.semantic.colorAttach0.fmt );
		}
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.colorAttach0.samples );
		attachments[ count ].loadOp = passState.semantic.colorAttach0.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].storeOp = passState.semantic.colorAttach0.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.colorAttach0.presentAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
		else if ( passState.semantic.colorAttach0.readAfter ) {
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
		attachments[ count ].loadOp = passState.semantic.colorAttach1.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.colorAttach1.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.colorAttach1.readAfter ) {
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
		attachments[ count ].loadOp = passState.semantic.colorAttach2.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.colorAttach2.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.colorAttach1.readAfter ) {
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
		attachments[ count ].loadOp = passState.semantic.depthAttach.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.depthAttach.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.depthAttach.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.depthAttach.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.depthAttach.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		depthAttachmentRef.attachment = count;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_STENCIL ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.stencilAttach.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.stencilAttach.samples );
		attachments[ count ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.stencilAttach.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.stencilAttach.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if ( passState.semantic.stencilAttach.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		stencilAttachmentRef.attachment = count;
		stencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		++count;
	}

	VkSubpassDescription subpass{ };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorCount;
	subpass.pColorAttachments = colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

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
	// Can specify clear options, etc. Assigned to cached render pass that matches
	VkImageView attachments[ 5 ] = {};
	uint32_t attachmentCount = 0;

	vk_RenderPassBits_t passBits = {};

	if( createInfo.color0 != nullptr )
	{
		passBits.semantic.colorAttach0.samples = createInfo.color0->info.subsamples;
		passBits.semantic.colorAttach0.fmt = createInfo.color0->info.fmt;
		passBits.semantic.colorAttach0.clear = 1;
		passBits.semantic.colorAttach0.store = 1;
		passBits.semantic.colorAttach0.readAfter = 1;
		passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR0;

		attachments[ attachmentCount ] = createInfo.color0->gpuImage->GetVkImageView();
		++attachmentCount;
	}

	if ( createInfo.color1 != nullptr )
	{
		passBits.semantic.colorAttach1.samples = createInfo.color1->info.subsamples;
		passBits.semantic.colorAttach1.fmt = createInfo.color1->info.fmt;
		passBits.semantic.colorAttach1.clear = 1;
		passBits.semantic.colorAttach1.store = 1;
		passBits.semantic.colorAttach1.readAfter = 1;
		passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR1;

		attachments[ attachmentCount ] = createInfo.color1->gpuImage->GetVkImageView();
		++attachmentCount;
	}

	if ( createInfo.color2 != nullptr )
	{
		passBits.semantic.colorAttach2.samples = createInfo.color2->info.subsamples;
		passBits.semantic.colorAttach2.fmt = createInfo.color2->info.fmt;
		passBits.semantic.colorAttach2.clear = 1;
		passBits.semantic.colorAttach2.store = 1;
		passBits.semantic.colorAttach2.readAfter = 1;
		passBits.semantic.attachmentMask |= RENDER_PASS_MASK_COLOR2;

		attachments[ attachmentCount ] = createInfo.color2->gpuImage->GetVkImageView();
		++attachmentCount;
	}

	if ( createInfo.depth != nullptr )
	{
		passBits.semantic.depthAttach.samples = createInfo.depth->info.subsamples;
		passBits.semantic.depthAttach.fmt = createInfo.depth->info.fmt;
		passBits.semantic.depthAttach.clear = 1;
		passBits.semantic.depthAttach.store = 1;
		passBits.semantic.depthAttach.readAfter = 1;
		passBits.semantic.attachmentMask |= RENDER_PASS_MASK_DEPTH;

		attachments[ attachmentCount ] = createInfo.depth->gpuImage->GetVkImageView();
		++attachmentCount;
	}

	if ( createInfo.stencil != nullptr )
	{
		passBits.semantic.stencilAttach.samples = createInfo.stencil->info.subsamples;
		passBits.semantic.stencilAttach.fmt = createInfo.stencil->info.fmt;
		passBits.semantic.stencilAttach.clear = 1;
		passBits.semantic.stencilAttach.store = 1;
		passBits.semantic.stencilAttach.readAfter = 1;
		passBits.semantic.attachmentMask |= RENDER_PASS_MASK_STENCIL;

		attachments[ attachmentCount ] = createInfo.stencil->gpuImage->GetVkImageView();
		++attachmentCount;
	}

	renderPass = vk_CreateRenderPass( passBits );
	if ( renderPass == VK_NULL_HANDLE ) {
		throw std::runtime_error( "Failed to create framebuffer!" );
	}

	VkFramebufferCreateInfo framebufferInfo{ };
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = attachmentCount;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = createInfo.width;
	framebufferInfo.height = createInfo.height;
	framebufferInfo.layers = 1;

	if ( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &buffer ) != VK_SUCCESS ) {
		throw std::runtime_error( "Failed to create framebuffer!" );
	}

	color0 = createInfo.color0;
	color1 = createInfo.color1;
	color2 = createInfo.color2;
	depth = createInfo.depth;
	stencil = createInfo.stencil;
	width = createInfo.width;
	height = createInfo.height;;
}


void FrameBuffer::Destroy()
{
	vkDestroyFramebuffer( context.device, buffer, nullptr );
	vkDestroyRenderPass( context.device, renderPass, nullptr );
}