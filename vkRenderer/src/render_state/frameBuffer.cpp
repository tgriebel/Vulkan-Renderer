#include <gfxcore/asset_types/texture.h>
#include "frameBuffer.h"
#include "deviceContext.h"
#include "rhi.h"
#include "../render_core/swapChain.h"

extern SwapChain g_swapChain;

struct renderTransitionBits_t
{
	renderPassTransition_t			colorTrans0;
	renderPassTransition_t			colorTrans1;
	renderPassTransition_t			colorTrans2;
	renderPassTransition_t			depthTrans;
	renderPassTransition_t			stencilTrans;
};


struct vk_RenderPassBits_t
{
	union
	{
		struct vkRenderPassState_t
		{
			renderAttachmentBits_t		attachmentBits;
			renderTransitionBits_t		transitionBits;
			renderPassAttachmentMask_t	attachmentMask; // Mask for which attachments are used
		} semantic;
		uint8_t bytes[ VkPassBitsSize ];
	};

	vk_RenderPassBits_t()
	{
		memset( bytes, 0, VkPassBitsSize );
	}
};
static_assert( sizeof( vk_RenderPassBits_t ) == VkPassBitsSize, "Bits overflowed" );


struct renderPassTuple_t
{
	VkRenderPass		pass;
	vk_RenderPassBits_t	state;
};


static std::unordered_map<uint64_t, renderPassTuple_t> renderPassCache;

VkRenderPass vk_CreateRenderPass( const vk_RenderPassBits_t& passState )
{
	const uint64_t passHash = Hash( passState.bytes, VkPassBitsSize );
	auto it = renderPassCache.find( passHash );
	if( it != renderPassCache.end() )
	{
		assert( memcmp( passState.bytes, it->second.state.bytes, VkPassBitsSize ) == 0 );
		return it->second.pass;
	}

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
		if ( passState.semantic.transitionBits.colorTrans0.flags.presentAfter ) {
			attachments[ count ].format = vk_GetTextureFormat( g_swapChain.GetBackBufferFormat() );
		}
		else {
			attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.color0.fmt );
		}
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.color0.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.colorTrans0.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.colorTrans0.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if( passState.semantic.transitionBits.colorTrans0.flags.presentBefore ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		} else if ( passState.semantic.transitionBits.colorTrans0.flags.readOnly ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.colorTrans0.flags.presentAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		} else if ( passState.semantic.transitionBits.colorTrans0.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR1 ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.color1.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.color1.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.colorTrans1.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.colorTrans1.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if ( passState.semantic.transitionBits.colorTrans1.flags.readOnly ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.colorTrans1.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_COLOR2 ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.color2.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.color2.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.colorTrans2.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.colorTrans2.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		if ( passState.semantic.transitionBits.colorTrans2.flags.readOnly ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.colorTrans2.flags.readAfter ) {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		colorAttachmentRef[ count ].attachment = count;
		colorAttachmentRef[ count ].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		++count;
	}

	colorCount = count;

	if ( ( passState.semantic.attachmentMask & RENDER_PASS_MASK_DEPTH ) != 0 )
	{
		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.depth.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.depth.samples );
		attachments[ count ].loadOp = passState.semantic.transitionBits.depthTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].storeOp = passState.semantic.transitionBits.depthTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.transitionBits.depthTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.transitionBits.depthTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		if ( passState.semantic.transitionBits.depthTrans.flags.readOnly ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.depthTrans.flags.readAfter ) {
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

		attachments[ count ].format = vk_GetTextureFormat( passState.semantic.attachmentBits.stencil.fmt );
		attachments[ count ].samples = vk_GetSampleCount( passState.semantic.attachmentBits.stencil.samples );
		attachments[ count ].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[ count ].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[ count ].stencilLoadOp = passState.semantic.transitionBits.stencilTrans.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[ count ].stencilStoreOp = passState.semantic.transitionBits.stencilTrans.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		
		if ( passState.semantic.transitionBits.stencilTrans.flags.readOnly ) {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		} else {
			attachments[ count ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		if ( passState.semantic.transitionBits.stencilTrans.flags.readAfter ) {
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

	VK_CHECK_RESULT( vkCreateRenderPass( context.device, &renderPassInfo, nullptr, &pass ) );

	renderPassCache[ passHash ] = renderPassTuple_t{ pass, passState };

	return pass;
}


void vk_ClearRenderPassCache()
{
	for( auto it : renderPassCache ) {
		if ( it.second.pass != VK_NULL_HANDLE ) {
			vkDestroyRenderPass( context.device, it.second.pass, nullptr );
		}
	}
}


void FrameBuffer::Create( const frameBufferCreateInfo_t& createInfo )
{
	if ( m_bufferCount > 0 ) {
		throw std::runtime_error( "Framebuffer already initialized." );
	}

	renderPassTransition_t perms[ PassPermCount ];
	for ( uint32_t i = 0; i < PassPermCount; ++i ) {
		perms[ i ].bits = i;
	}

	m_bufferCount = ( createInfo.lifetime == swapBuffering_t::MULTI_FRAME ) ? MaxFrameStates : 1;
	const bool canPresent = ( createInfo.color0 != nullptr ) && ( createInfo.color0->info.fmt == g_swapChain.GetBackBufferFormat() );

	m_colorCount += ( createInfo.color0 != nullptr ) ? 1 : 0;
	m_colorCount += ( createInfo.color1 != nullptr ) ? 1 : 0;
	m_colorCount += ( createInfo.color2 != nullptr ) ? 1 : 0;
	m_dsCount += ( createInfo.depth != nullptr ) ? 1 : 0;
	m_dsCount += ( createInfo.stencil != nullptr ) ? 1 : 0;

	m_attachmentCount = m_colorCount + m_dsCount;

	Image* images[ MaxAttachmentCount ];
	images[ 0 ] = createInfo.color0;
	images[ 1 ] = createInfo.color1;
	images[ 2 ] = createInfo.color2;
	images[ 3 ] = createInfo.depth;
	images[ 4 ] = createInfo.stencil;

	uint32_t firstValidIx = MaxAttachmentCount;
	for ( uint32_t imageIx = 0; imageIx < MaxAttachmentCount; ++imageIx ) {
		if ( images[ imageIx ] != nullptr ) {
			firstValidIx = imageIx;
			break;
		}
	}

	// Validation
	{
		if( firstValidIx == MaxAttachmentCount ) {
			throw std::runtime_error( "No images provided." );
		}

		if ( ( createInfo.color0 == nullptr ) &&
			( ( createInfo.color1 != nullptr ) || ( createInfo.color2 != nullptr ) ) ) {
			throw std::runtime_error( "Color attachment 0 has to be used if 1 and 2 are." );
		}

		if ( ( createInfo.color2 != nullptr ) && ( createInfo.color1 != nullptr ) ) {
			throw std::runtime_error( "Color attachment 1 has to be used if 2 is." );
		}

		for ( uint32_t imageIx = firstValidIx + 1; imageIx < MaxAttachmentCount; ++imageIx )
		{
			if( images[ imageIx ] == nullptr ) {
				continue;
			}
			if( images[ firstValidIx ]->info.width != images[ imageIx ]->info.width ||
				images[ firstValidIx ]->info.height != images[ imageIx ]->info.height || 
				images[ firstValidIx ]->info.layers != images[ imageIx ]->info.layers )
			{
				throw std::runtime_error( "Framebuffer images must have the same dimensions." );
			}
		}
	}

	// Defaults
	for ( uint32_t frameIx = 0; frameIx < m_bufferCount; ++frameIx ) {
		for ( uint32_t permIx = 0; permIx < PassPermCount; ++permIx ) {	
			vk_buffers[ frameIx ][ permIx ] = VK_NULL_HANDLE;
		}
	}
	for ( uint32_t permIx = 0; permIx < PassPermCount; ++permIx ) {
		vk_renderPasses[ permIx ] = VK_NULL_HANDLE;
	}

	// Attachment bits
	m_attachmentBits = {};
	renderPassAttachmentMask_t mask = RENDER_PASS_MASK_NONE;
	{
		if ( createInfo.color0 != nullptr )
		{
			m_attachmentBits.color0.samples = createInfo.color0->info.subsamples;
			m_attachmentBits.color0.fmt = createInfo.color0->info.fmt;
			mask |= RENDER_PASS_MASK_COLOR0;
		}
		if ( createInfo.color1 != nullptr )
		{
			m_attachmentBits.color1.samples = createInfo.color1->info.subsamples;
			m_attachmentBits.color1.fmt = createInfo.color1->info.fmt;
			mask |= RENDER_PASS_MASK_COLOR1;
		}
		if ( createInfo.color2 != nullptr )
		{
			m_attachmentBits.color2.samples = createInfo.color2->info.subsamples;
			m_attachmentBits.color2.fmt = createInfo.color2->info.fmt;
			mask |= RENDER_PASS_MASK_COLOR2;
		}

		if ( createInfo.depth != nullptr )
		{
			m_attachmentBits.depth.samples = createInfo.depth->info.subsamples;
			m_attachmentBits.depth.fmt = createInfo.depth->info.fmt;
			mask |= RENDER_PASS_MASK_DEPTH;
		}

		if ( createInfo.stencil != nullptr )
		{
			m_attachmentBits.stencil.samples = createInfo.stencil->info.subsamples;
			m_attachmentBits.stencil.fmt = createInfo.stencil->info.fmt;
			mask |= RENDER_PASS_MASK_STENCIL;
		}
	}

	// Initialization
	for( uint32_t permIx = 0; permIx < PassPermCount; ++permIx )
	{
		const renderPassTransition_t& state = perms[ permIx ];

		if( ( canPresent == false ) && state.flags.presentAfter ) {
			continue;
		}

		// Can specify clear options, etc. Assigned to cached render pass that matches
		vk_RenderPassBits_t passBits = {};
		passBits.semantic.attachmentBits = m_attachmentBits;

		passBits.semantic.transitionBits.colorTrans0 = state;
		passBits.semantic.transitionBits.colorTrans1 = state;
		passBits.semantic.transitionBits.colorTrans2 = state;
		passBits.semantic.transitionBits.depthTrans = state;
		passBits.semantic.transitionBits.stencilTrans = state;
		passBits.semantic.attachmentMask = mask;

		vk_renderPasses[ permIx ] = vk_CreateRenderPass( passBits );
		if ( vk_renderPasses[ permIx ] == VK_NULL_HANDLE ) {
			throw std::runtime_error( "Failed to create framebuffer! Render pass invalid" );
		}

		for ( uint32_t frameIx = 0; frameIx < m_bufferCount; ++frameIx )
		{
			VkImageView attachments[ MaxAttachmentCount ] = {};
			
			uint32_t currentAttachment = 0;
			if ( createInfo.color0 != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.color0->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.color1 != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.color1->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.color2 != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.color2->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.depth != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.depth->gpuImage->GetVkImageView( frameIx );
			}
			if ( createInfo.stencil != nullptr ) {
				attachments[ currentAttachment++ ] = createInfo.stencil->gpuImage->GetVkImageView( frameIx );
			}
			assert( currentAttachment == m_attachmentCount );

			VkFramebufferCreateInfo framebufferInfo{ };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = vk_renderPasses[ permIx ];
			framebufferInfo.attachmentCount = m_attachmentCount;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = images[ firstValidIx ]->info.width;
			framebufferInfo.height = images[ firstValidIx ]->info.height;
			framebufferInfo.layers = images[ firstValidIx ]->subResourceView.arrayCount;

			VK_CHECK_RESULT( vkCreateFramebuffer( context.device, &framebufferInfo, nullptr, &vk_buffers[ frameIx ][ permIx ] ) );

			vk_MarkerSetObjectName( (uint64_t)vk_buffers[ frameIx ][ permIx ], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, createInfo.name );
		}
		m_color0 = createInfo.color0;
		m_color1 = createInfo.color1;
		m_color2 = createInfo.color2;
		m_depth = createInfo.depth;
		m_stencil = createInfo.stencil;
	}
	m_width = images[ firstValidIx ]->info.width;
	m_height = images[ firstValidIx ]->info.height;
	m_lifetime = createInfo.lifetime;
	
	if( m_color0 == nullptr ) {
		assert( ( m_color1 == nullptr ) && ( m_color2 == nullptr ) );
	}
}


void FrameBuffer::Destroy()
{
	assert( context.device != VK_NULL_HANDLE );
	if ( context.device != VK_NULL_HANDLE )
	{	
		for ( uint32_t frameIx = 0; frameIx < m_bufferCount; ++frameIx )
		{
			for ( uint32_t i = 0; i < PassPermCount; ++i )
			{
				if ( vk_buffers != VK_NULL_HANDLE )
				{
					vkDestroyFramebuffer( context.device, vk_buffers[ frameIx ][ i ], nullptr );
					vk_buffers[ frameIx ][ i ] = VK_NULL_HANDLE;
				}		
			}
			
		}
		for ( uint32_t i = 0; i < PassPermCount; ++i ) {
			vk_renderPasses[ i ] = VK_NULL_HANDLE;
		}
	}
	m_colorCount = 0;
	m_dsCount = 0;
	m_attachmentCount = 0;
	m_bufferCount = 0;
}