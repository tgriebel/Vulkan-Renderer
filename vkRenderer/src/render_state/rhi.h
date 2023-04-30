#pragma once

#include <resource_types/texture.h>
#include "../render_state/deviceContext.h"
#include "../render_binding/shaderBinding.h"

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


static inline VkImageViewType vk_GetTextureType( textureType_t type )
{
	switch ( type ) {
		default:
		case TEXTURE_TYPE_2D:			return VK_IMAGE_VIEW_TYPE_2D;
		case TEXTURE_TYPE_2D_ARRAY:		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case TEXTURE_TYPE_3D:			return VK_IMAGE_VIEW_TYPE_3D;
		case TEXTURE_TYPE_CUBE:			return VK_IMAGE_VIEW_TYPE_CUBE;
		case TEXTURE_TYPE_CUBE_ARRAY:	return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	}
	assert(0);
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
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


static VkDescriptorType vk_GetDescriptorType( const bindType_t type )
{
	switch ( type )
	{
		case CONSTANT_BUFFER:		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case IMAGE_2D:				return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case IMAGE_3D:				return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case IMAGE_CUBE:			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case READ_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case WRITE_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case READ_IMAGE_BUFFER:		return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		case WRITE_IMAGE_BUFFER:	return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		default: break;
	}
	assert( 0 );
	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}


static VkShaderStageFlagBits vk_GetStageFlags( const bindStateFlag_t flags )
{
	uint32_t count = 0;
	uint32_t bits = flags;

	uint32_t vkFlags = 0;

	while ( bits )
	{
		uint32_t bitFlag = bits & 0x1;
		switch ( bitFlag )
		{
		case BIND_STATE_VS:		vkFlags |= VK_SHADER_STAGE_VERTEX_BIT;		break;
		case BIND_STATE_PS:		vkFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;	break;
		case BIND_STATE_CS:		vkFlags |= VK_SHADER_STAGE_COMPUTE_BIT;		break;
		default:
		case BIND_STATE_ALL:	vkFlags |= VK_SHADER_STAGE_ALL;				break;
		}
		bits &= ( bits - 1 );
		count++;
	}
	return VkShaderStageFlagBits( vkFlags );
}