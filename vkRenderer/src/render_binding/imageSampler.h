#pragma once



#include "../render_core/gpuImage.h"
#include "../render_state/deviceContext.h"
#include "../asset_types/texture.h"

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
