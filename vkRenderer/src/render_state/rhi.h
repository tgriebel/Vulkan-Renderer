#pragma once

#include <gfxcore/asset_types/texture.h>
#include "../render_state/deviceContext.h"
#include "../render_binding/shaderBinding.h"

enum renderPassAttachmentMask_t : uint8_t
{
	RENDER_PASS_MASK_NONE = 0,
	RENDER_PASS_MASK_COLOR0 = ( 1 << 0 ),
	RENDER_PASS_MASK_COLOR1 = ( 1 << 1 ),
	RENDER_PASS_MASK_COLOR2 = ( 1 << 2 ),
	RENDER_PASS_MASK_DEPTH = ( 1 << 3 ),
	RENDER_PASS_MASK_STENCIL = ( 1 << 4 ),
};
DEFINE_ENUM_OPERATORS( renderPassAttachmentMask_t, uint8_t )

// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
struct renderPassAttachmentBits_t
{
	imageSamples_t	samples : 8;
	imageFmt_t		fmt		: 8;
};
static_assert( sizeof( renderPassAttachmentBits_t ) == 2, "Bits overflowed" );


struct renderAttachmentBits_t
{
	renderPassAttachmentBits_t	color0;
	renderPassAttachmentBits_t	color1;
	renderPassAttachmentBits_t	color2;
	renderPassAttachmentBits_t	depth;
	renderPassAttachmentBits_t	stencil;
};
static_assert( sizeof( renderAttachmentBits_t ) == 10, "Bits overflowed" );


union renderPassTransition_t
{
	struct renderPassStateBits_t
	{
		uint8_t	clear			: 1;
		uint8_t	store			: 1;
		uint8_t	readAfter		: 1;
		uint8_t	presentAfter	: 1;
		uint8_t	readOnly		: 1;
		uint8_t	presentBefore	: 1;
	} flags;
	uint8_t						bits;
};
static_assert( sizeof( imageSamples_t ) == 1, "Bits overflowed" );
static_assert( sizeof( imageFmt_t ) == 1, "Bits overflowed" );
static_assert( sizeof( renderPassAttachmentBits_t ) == 2, "Bits overflowed" );
static_assert( sizeof( renderPassTransition_t ) == 1, "Bits overflowed" );

static const uint32_t PassPermBits = 6;
static const uint32_t PassPermCount = ( 1 << PassPermBits );

static const uint32_t VkPassBitsSize = 16;

struct vk_RenderPassBits_t;
VkRenderPass vk_CreateRenderPass( const vk_RenderPassBits_t& passState );
void vk_ClearRenderPassCache();

struct vk_formatTableEntry_t
{
	imageFmt_t	imgFmt;
	VkFormat	vk_imgFmt;
};

static const vk_formatTableEntry_t vk_formatTable[] =
{
	{ IMAGE_FMT_UNKNOWN,		VK_FORMAT_UNDEFINED				},
	{ IMAGE_FMT_R_8,			VK_FORMAT_R8_SRGB				},
	{ IMAGE_FMT_R_16,			VK_FORMAT_R16_SFLOAT			},
	{ IMAGE_FMT_D_16,			VK_FORMAT_D16_UNORM				},
	{ IMAGE_FMT_D24S8,			VK_FORMAT_D24_UNORM_S8_UINT		},
	{ IMAGE_FMT_D_32,			VK_FORMAT_D32_SFLOAT			},
	{ IMAGE_FMT_D_32_S8,		VK_FORMAT_D32_SFLOAT_S8_UINT	},
	{ IMAGE_FMT_RGB_8,			VK_FORMAT_R8G8B8_SRGB			},
	{ IMAGE_FMT_RGBA_8,			VK_FORMAT_R8G8B8A8_SRGB			},
	{ IMAGE_FMT_RGBA_8_UNORM,	VK_FORMAT_R8G8B8A8_UNORM		},
	{ IMAGE_FMT_ABGR_8,			VK_FORMAT_A8B8G8R8_SRGB_PACK32	},
	{ IMAGE_FMT_BGR_8,			VK_FORMAT_B8G8R8_SRGB			},
	{ IMAGE_FMT_BGRA_8,			VK_FORMAT_B8G8R8A8_SRGB			},
	{ IMAGE_FMT_RGB_16,			VK_FORMAT_R16G16B16_SFLOAT		},
	{ IMAGE_FMT_RGBA_16,		VK_FORMAT_R16G16B16A16_SFLOAT	},
	{ IMAGE_FMT_RG_32,			VK_FORMAT_R32G32_SFLOAT			},
};

static inline constexpr VkFormat vk_GetTextureFormat( const imageFmt_t fmt )
{
	for( uint32_t i = 0; i < COUNTARRAY( vk_formatTable ); ++i ) {
		if( vk_formatTable[i].imgFmt == fmt ) {
			return vk_formatTable[ i ].vk_imgFmt;
		}
	}
	return VK_FORMAT_UNDEFINED;
}


static inline constexpr imageFmt_t vk_GetTextureFormat( const VkFormat fmt )
{
	for ( uint32_t i = 0; i < COUNTARRAY( vk_formatTable ); ++i ) {
		if ( vk_formatTable[ i ].vk_imgFmt == fmt ) {
			return vk_formatTable[ i ].imgFmt;
		}
	}
	return IMAGE_FMT_UNKNOWN;
}


static inline VkImageAspectFlagBits vk_GetAspectFlags( const imageAspectFlags_t flags )
{
	uint32_t bitMask = 0x01;

	uint32_t vkFlags = 0;

	while ( bitMask < IMAGE_ASPECT_ALL )
	{
		uint32_t bitFlag = flags & bitMask;
		switch ( bitFlag )
		{
			case IMAGE_ASPECT_COLOR_FLAG:		vkFlags |= VK_IMAGE_ASPECT_COLOR_BIT;	break;
			case IMAGE_ASPECT_DEPTH_FLAG:		vkFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;	break;
			case IMAGE_ASPECT_STENCIL_FLAG:		vkFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;	break;
		}
		bitMask <<= 1;
	}
	return VkImageAspectFlagBits( vkFlags );
}


static inline VkImageViewType vk_GetImageViewType( const imageType_t type )
{
	switch ( type ) {
	default:
		case IMAGE_TYPE_2D:				return VK_IMAGE_VIEW_TYPE_2D;
		case IMAGE_TYPE_2D_ARRAY:		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case IMAGE_TYPE_3D:				return VK_IMAGE_VIEW_TYPE_3D;
		case IMAGE_TYPE_CUBE:			return VK_IMAGE_VIEW_TYPE_CUBE;
		case IMAGE_TYPE_CUBE_ARRAY:		return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	}
	assert( 0 );
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}


static inline VkSampleCountFlagBits vk_GetSampleCount( const imageSamples_t sampleCount )
{
	switch ( sampleCount )
	{
		case IMAGE_SMP_1:			return VK_SAMPLE_COUNT_1_BIT;
		case IMAGE_SMP_2:			return VK_SAMPLE_COUNT_2_BIT;
		case IMAGE_SMP_4:			return VK_SAMPLE_COUNT_4_BIT;
		case IMAGE_SMP_8:			return VK_SAMPLE_COUNT_8_BIT;
		case IMAGE_SMP_16:			return VK_SAMPLE_COUNT_16_BIT;
		case IMAGE_SMP_32:			return VK_SAMPLE_COUNT_32_BIT;
		case IMAGE_SMP_64:			return VK_SAMPLE_COUNT_64_BIT;
		default: assert( false );	break;
	}
	return VK_SAMPLE_COUNT_1_BIT;
}


static VkDescriptorType vk_GetDescriptorType( const bindType_t type )
{
	switch ( type )
	{
		case bindType_t::CONSTANT_BUFFER:		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case bindType_t::IMAGE_2D:				return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case bindType_t::IMAGE_2D_ARRAY:		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case bindType_t::IMAGE_3D:				return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case bindType_t::IMAGE_CUBE:			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case bindType_t::IMAGE_CUBE_ARRAY:		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case bindType_t::READ_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case bindType_t::WRITE_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case bindType_t::READ_IMAGE_BUFFER:		return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		case bindType_t::WRITE_IMAGE_BUFFER:	return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		default: break;
	}
	assert( 0 );
	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}


static VkShaderStageFlagBits vk_GetStageFlags( const bindStateFlag_t flags )
{
	uint32_t bitMask = 0x01;

	uint32_t vkFlags = 0;

	while ( bitMask < IMAGE_ASPECT_ALL )
	{
		uint32_t bitFlag = flags & bitMask;
		switch ( bitFlag )
		{
			case BIND_STATE_VS:		vkFlags |= VK_SHADER_STAGE_VERTEX_BIT;		break;
			case BIND_STATE_PS:		vkFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;	break;
			case BIND_STATE_CS:		vkFlags |= VK_SHADER_STAGE_COMPUTE_BIT;		break;
			default:
			case BIND_STATE_ALL:	vkFlags |= VK_SHADER_STAGE_ALL;				break;
		}
		bitMask <<= 1;
	}
	return VkShaderStageFlagBits( vkFlags );
}