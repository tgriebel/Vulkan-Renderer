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

#include <GfxCore/asset_types/texture.h>
#include "../render_core/gpuImage.h"
#include "../render_state/deviceContext.h"

class ImageSampler : public RenderResource
{
private:
	samplerState_t	m_samplerState;
#ifdef USE_VULKAN
	VkSampler		vk_sampler;
#endif

public:
	ImageSampler()
	{
	}

	~ImageSampler()
	{
		// FIXME: TODO:
	}

#ifdef USE_VULKAN
	inline const VkSampler GetVkObject() const { return vk_sampler; }
#endif

	void Init( const samplerState_t& state, const resourceLifeTime_t lifetime );

	void Destroy() override;
};
