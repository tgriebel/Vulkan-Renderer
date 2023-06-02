#include <algorithm>
#include <iterator>
#include <numeric>
#include <map>
#include "../render_core/renderer.h"
#include <scene/scene.h>
#include <scene/entity.h>
#include <sstream>
#include "../render_core/debugMenu.h"
#include "../render_core/gpuImage.h"
#include "../render_state/rhi.h"
#include "bindings.h"

union descriptorInfo_t
{
	VkDescriptorBufferInfo	bufferInfo;
	VkDescriptorImageInfo	imageInfo;
};

class DescriptorWritesBuilder
{
private:
	static const uint32_t MaxPoolSize = 100;
	uint32_t							bufferInfoPoolFreeList = 0;
	uint32_t							imageInfoPoolFreeList = 0;
	uint32_t							bufferInfoArrayPoolFreeList = 0;
	uint32_t							imageInfoArrayPoolFreeList = 0;
	VkDescriptorBufferInfo				bufferInfoPool[ MaxPoolSize ];
	VkDescriptorImageInfo				imageInfoPool[ MaxPoolSize ];
	std::vector<VkDescriptorBufferInfo>	bufferInfoArrayPool[ MaxPoolSize ];
	std::vector<VkDescriptorImageInfo>	imageInfoArrayPool[ MaxPoolSize ];
public:
	DescriptorWritesBuilder()
	{
		const uint32_t reservationSize = 128;
		for ( uint32_t i = 0; i < MaxPoolSize; ++i )
		{
			bufferInfoArrayPool[ i ].reserve( reservationSize );
			imageInfoArrayPool[ i ].reserve( reservationSize );
		}
		bufferInfoPoolFreeList = 0;
		imageInfoPoolFreeList = 0;
		bufferInfoArrayPoolFreeList = 0;
		imageInfoArrayPoolFreeList = 0;
	}

	void Reset()
	{
		for ( uint32_t i = 0; i < MaxPoolSize; ++i )
		{
			bufferInfoArrayPool[ i ].resize( 0 );
			imageInfoArrayPool[ i ].resize( 0 );
		}
		bufferInfoPoolFreeList = 0;
		imageInfoPoolFreeList = 0;
		bufferInfoArrayPoolFreeList = 0;
		imageInfoArrayPoolFreeList = 0;
	}

	[[nodiscard]]
	std::vector<VkDescriptorBufferInfo>& NextBufferInfoArray()
	{
		return bufferInfoArrayPool[ bufferInfoArrayPoolFreeList++ ];
	}

	[[nodiscard]]
	std::vector<VkDescriptorImageInfo>& NextImageInfoArray()
	{
		return imageInfoArrayPool[ imageInfoArrayPoolFreeList++ ];
	}

	[[nodiscard]]
	VkDescriptorBufferInfo& NextBufferInfo()
	{
		bufferInfoPool[ bufferInfoPoolFreeList ] = {};
		return bufferInfoPool[ bufferInfoPoolFreeList++ ];
	}

	[[nodiscard]]
	VkDescriptorImageInfo& NextImageInfo()
	{
		imageInfoPool[ imageInfoPoolFreeList ] = {};
		return imageInfoPool[ imageInfoPoolFreeList++ ];
	}
};

static DescriptorWritesBuilder writeBuilder;

void Renderer::AppendDescriptorWrites( const ShaderBindParms& parms, std::vector<VkWriteDescriptorSet>& descSetWrites )
{
	const ShaderBindSet* set = parms.GetSet();

	const uint32_t count = set->Count();
	descSetWrites.reserve( descSetWrites.size() + count );

	for ( uint32_t i = 0; i < count; ++i )
	{
		const ShaderBinding* binding = set->GetBinding( i );
		const ShaderAttachment* attachment = parms.GetAttachment( binding );

		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.descriptorCount = binding->GetMaxDescriptorCount();
		writeInfo.dstSet = parms.GetVkObject();
		writeInfo.descriptorType = vk_GetDescriptorType( binding->GetType() );
		writeInfo.dstArrayElement = 0;
		writeInfo.dstBinding = binding->GetSlot();

		if ( attachment->GetType() == ShaderAttachment::type_t::BUFFER ) {
			const GpuBuffer* buffer = attachment->GetBuffer();

			VkDescriptorBufferInfo& info = writeBuilder.NextBufferInfo();
			info.buffer = buffer->GetVkObject();
			info.offset = buffer->GetBaseOffset();
			info.range = buffer->GetSize();

			assert( info.buffer != nullptr );

			writeInfo.pBufferInfo = &info;
		}
		else if ( attachment->GetType() == ShaderAttachment::type_t::IMAGE ) {
			const Image* image = attachment->GetImage();

			VkDescriptorImageInfo& info = writeBuilder.NextImageInfo();
			info.sampler = vk_bilinearSampler;
			info.imageView = attachment->GetImage()->gpuImage->GetVkImageView();
			assert( info.imageView != nullptr );

			if ( ( image->info.aspect & ( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) ) != 0 ) {
				info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else {
				info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			writeInfo.pImageInfo = &info;
		}
		else if ( attachment->GetType() == ShaderAttachment::type_t::IMAGE_ARRAY ) {
			std::vector<VkDescriptorImageInfo>& infos = writeBuilder.NextImageInfoArray();

			const ImageArray& images = *attachment->GetImageArray();

			const uint32_t imageCount = images.Count();
			infos.resize( imageCount );
			writeInfo.descriptorCount = imageCount;

			assert( imageCount <= binding->GetMaxDescriptorCount() );

			for ( uint32_t imageIx = 0; imageIx < imageCount; ++imageIx )
			{
				const Image* image = images[ imageIx ];
				VkDescriptorImageInfo& info = infos[ imageIx ];

				info = {};
				info.imageView = images[ imageIx ]->gpuImage->GetVkImageView();
				assert( info.imageView != nullptr );

				if ( ( image->info.aspect & ( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) ) != 0 )
				{
					info.sampler = vk_depthShadowSampler;
					info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}
				else
				{
					info.sampler = vk_bilinearSampler;
					info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
			}
			writeInfo.pImageInfo = infos.data();
		}

		descSetWrites.push_back( writeInfo );
	}
}


void Renderer::UpdateFrameDescSet( const int currentImage )
{
	writeBuilder.Reset();
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	AppendDescriptorWrites( *particleState.parms[ currentImage ], descriptorWrites );

	for( uint32_t i = 0; i < DRAWPASS_COUNT; ++i )
	{
		if ( shadowView.passes[ i ] != nullptr ) {
			AppendDescriptorWrites( *shadowView.passes[ i ]->parms[ currentImage ], descriptorWrites );
		}
		if ( renderView.passes[ i ] != nullptr ) {
			AppendDescriptorWrites( *renderView.passes[ i ]->parms[ currentImage ], descriptorWrites );
		}
		if ( view2D.passes[ i ] != nullptr ) {
			AppendDescriptorWrites( *view2D.passes[ i ]->parms[ currentImage ], descriptorWrites );
		}
	}

	vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );
}