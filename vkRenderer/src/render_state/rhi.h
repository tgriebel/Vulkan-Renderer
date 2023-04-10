#pragma once

#include <resource_types/texture.h>
#include "../render_state/deviceContext.h"

static inline VkFormat vk_GetTextureFormat( textureFmt_t fmt )
{
	switch ( fmt )
	{
		case TEXTURE_FMT_UNKNOWN:	return VK_FORMAT_UNDEFINED;
		case TEXTURE_FMT_R_8:		return VK_FORMAT_R8_SRGB;
		case TEXTURE_FMT_R_16:		return VK_FORMAT_R16_SFLOAT;
		case TEXTURE_FMT_D_16:		return VK_FORMAT_D16_UNORM;
		case TEXTURE_FMT_D24S8:		return VK_FORMAT_D24_UNORM_S8_UINT;
		case TEXTURE_FMT_D_32:		return VK_FORMAT_D32_SFLOAT;
		case TEXTURE_FMT_D_32_S8:	return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case TEXTURE_FMT_RGB_8:		return VK_FORMAT_R8G8B8_SRGB;
		case TEXTURE_FMT_RGBA_8:	return VK_FORMAT_R8G8B8A8_SRGB;
		case TEXTURE_FMT_ABGR_8:	return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
		case TEXTURE_FMT_BGR_8:		return VK_FORMAT_B8G8R8_SRGB;
		case TEXTURE_FMT_BGRA_8:	return VK_FORMAT_B8G8R8A8_SRGB;
		case TEXTURE_FMT_RGB_16:	return VK_FORMAT_R16G16B16_SFLOAT;
		case TEXTURE_FMT_RGBA_16:	return VK_FORMAT_R16G16B16A16_SFLOAT;
		default: assert( false );	break;
	}
	return VK_FORMAT_R8G8B8A8_SRGB;
}


static inline VkSampleCountFlagBits vk_GetSampleCount( const textureSamples_t sampleCount )
{
	switch ( sampleCount )
	{
		case TEXTURE_SMP_1:			return VK_SAMPLE_COUNT_1_BIT;
		case TEXTURE_SMP_2:			return VK_SAMPLE_COUNT_2_BIT;
		case TEXTURE_SMP_4:			return VK_SAMPLE_COUNT_4_BIT;
		case TEXTURE_SMP_8:			return VK_SAMPLE_COUNT_8_BIT;
		case TEXTURE_SMP_16:		return VK_SAMPLE_COUNT_16_BIT;
		case TEXTURE_SMP_32:		return VK_SAMPLE_COUNT_32_BIT;
		case TEXTURE_SMP_64:		return VK_SAMPLE_COUNT_64_BIT;
		default: assert( false );	break;
	}
	return VK_SAMPLE_COUNT_1_BIT;
}