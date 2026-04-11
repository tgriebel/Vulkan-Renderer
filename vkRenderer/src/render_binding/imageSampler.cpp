#pragma once

/*
* MIT License
*
* Copyright( c ) 2026 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "imageSampler.h"
#include "../render_state/deviceContext.h"
#include "../render_state/rhi.h"

void ImageSampler::Init( const samplerState_t& state, const resourceLifeTime_t lifetime )
{
	{
		m_lifetime = lifetime;
		RenderResource::Create( resourceType_t::IMAGE_SAMPLER, m_lifetime );
	}
	m_samplerState = state;

#ifdef USE_VULKAN
	VkSamplerCreateInfo samplerInfo { };
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = ( state.filter != SAMPLER_FILTER_NEAREST ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	samplerInfo.minFilter = ( state.filter != SAMPLER_FILTER_NEAREST ) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	VkSamplerAddressMode samplerAddress = vk_GetSamplerAddress( m_samplerState.addrMode );

	samplerInfo.addressModeU = samplerAddress;
	samplerInfo.addressModeV = samplerAddress;
	samplerInfo.addressModeW = samplerAddress;
	samplerInfo.anisotropyEnable = ( state.maxAniso > 0.0f ) ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = Clamp( state.maxAniso, 1.0f, context.deviceProperties.limits.maxSamplerAnisotropy );
	samplerInfo.borderColor = vk_GetBorderColor( m_samplerState.borderColor, m_samplerState.borderTransparent, m_samplerState.borderColorIsFloat );
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = ( state.filter == SAMPLER_FILTER_TRILINEAR ) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.minLod = state.minLod;
	samplerInfo.maxLod = Clamp( state.maxLod, state.minLod, VK_LOD_CLAMP_NONE );
	samplerInfo.mipLodBias = 0.0f;

	assert( VK_LOD_CLAMP_NONE == 1000.0f ); // Check clamp if asserts

	VK_CHECK_RESULT( vkCreateSampler( context.device, &samplerInfo, nullptr, &vk_sampler ) );
#endif
}


void ImageSampler::Destroy()
{
	vkDestroySampler( context.device, vk_sampler, nullptr );
}
