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
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	VkSamplerAddressMode samplerAddress = vk_GetSamplerAddress( m_samplerState.addrMode );

	samplerInfo.addressModeU = samplerAddress;
	samplerInfo.addressModeV = samplerAddress;
	samplerInfo.addressModeW = samplerAddress;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 16.0f;
	samplerInfo.mipLodBias = 0.0f;

	VK_CHECK_RESULT( vkCreateSampler( context.device, &samplerInfo, nullptr, &vk_sampler ) );
#endif
}


void ImageSampler::Destroy()
{
	vkDestroySampler( context.device, vk_sampler, nullptr );
}
