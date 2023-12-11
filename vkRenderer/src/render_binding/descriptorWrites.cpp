#include <algorithm>
#include <iterator>
#include <numeric>
#include <map>
#include "../render_core/renderer.h"
#include "../globals/renderConstants.h"
#include <gfxcore/scene/scene.h>
#include <gfxcore/scene/entity.h>
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
	static const uint32_t MaxPoolSize = 512;
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
		assert( bufferInfoArrayPoolFreeList < MaxPoolSize );
		return bufferInfoArrayPool[ bufferInfoArrayPoolFreeList++ ];
	}

	[[nodiscard]]
	std::vector<VkDescriptorImageInfo>& NextImageInfoArray()
	{
		assert( imageInfoArrayPoolFreeList < MaxPoolSize );
		return imageInfoArrayPool[ imageInfoArrayPoolFreeList++ ];
	}

	[[nodiscard]]
	VkDescriptorBufferInfo& NextBufferInfo()
	{
		assert( bufferInfoPoolFreeList < MaxPoolSize );
		bufferInfoPool[ bufferInfoPoolFreeList ] = {};
		return bufferInfoPool[ bufferInfoPoolFreeList++ ];
	}

	[[nodiscard]]
	VkDescriptorImageInfo& NextImageInfo()
	{
		assert( imageInfoPoolFreeList < MaxPoolSize );
		imageInfoPool[ imageInfoPoolFreeList ] = {};
		return imageInfoPool[ imageInfoPoolFreeList++ ];
	}
};

static DescriptorWritesBuilder writeBuilder;

static void AppendDescriptorWrites( const ShaderBindParms& parms, const uint32_t currentBuffer, std::vector<VkWriteDescriptorSet>& descSetWrites )
{
	const ShaderBindSet* set = parms.GetSet();

	const uint32_t count = set->Count();
	descSetWrites.reserve( descSetWrites.size() + count );

	for ( uint32_t i = 0; i < count; ++i )
	{
		const ShaderBinding* binding = set->GetBinding( i );

		if( parms.AttachmentChanged( *binding ) == false ) {
			continue;
		}

		const ShaderAttachment* attachment = parms.GetAttachment( *binding );
		assert( attachment != nullptr );

		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.descriptorCount = binding->GetMaxDescriptorCount();
		writeInfo.dstSet = parms.GetVkObject();
		writeInfo.descriptorType = vk_GetDescriptorType( binding->GetType() );
		writeInfo.dstArrayElement = 0;
		writeInfo.dstBinding = binding->GetSlot();

		if ( attachment->GetSemantic() == bindSemantic_t::BUFFER ) {
			const GpuBuffer* buffer = attachment->GetBuffer();

			VkDescriptorBufferInfo& info = writeBuilder.NextBufferInfo();
			info.buffer = buffer->GetVkObject();
			info.offset = buffer->GetBaseOffset();
			info.range = buffer->GetSize();

			info.range = ( info.range == 0 ) ? VK_WHOLE_SIZE : info.range;

			assert( info.buffer != nullptr );

			writeInfo.pBufferInfo = &info;
		}
		else if ( attachment->GetSemantic() == bindSemantic_t::IMAGE ) {
			const Image* image = attachment->GetImage();
			if ( image == nullptr ) {
				image = rc.whiteImage;
			}

			VkDescriptorImageInfo& info = writeBuilder.NextImageInfo();
			info.sampler = context.bilinearSampler[ image->sampler.addrMode ];
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
		else if ( attachment->GetSemantic() == bindSemantic_t::IMAGE_ARRAY ) {
			std::vector<VkDescriptorImageInfo>& infos = writeBuilder.NextImageInfoArray();

			const ImageArray& images = *attachment->GetImageArray();

			const uint32_t descCount = binding->GetMaxDescriptorCount();
			const uint32_t imageCount = images.Count();
			assert( imageCount <= descCount );

			infos.resize( descCount );
			writeInfo.descriptorCount = descCount;

			for ( uint32_t descIx = 0; descIx < descCount; ++descIx )
			{
				const Image* image = rc.whiteImage;
				if ( ( descIx < imageCount ) && ( images[ descIx ] != nullptr ) ) {
					image = images[ descIx ];
				}

				VkDescriptorImageInfo& info = infos[ descIx ];

				info = {};
				info.imageView = image->gpuImage->GetVkImageView();
				assert( info.imageView != nullptr );

				if ( ( image->info.aspect & ( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) ) != 0 )
				{
					info.sampler = context.depthShadowSampler;
					info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}
				else
				{
					info.sampler = context.bilinearSampler[ image->sampler.addrMode ];
					info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
			}
			writeInfo.pImageInfo = infos.data();
		}

		descSetWrites.push_back( writeInfo );
	}
}


void RenderContext::UpdateBindParms()
{
	AllocRegisteredBindParms();

	writeBuilder.Reset();
	std::vector<VkWriteDescriptorSet> descriptorWrites;

	const uint32_t bindParmCount = bindParmsList.Count();
	for ( uint32_t i = 0; i < bindParmCount; ++i )
	{
		if( bindParmsList[ i ].IsValid() == false ) {
			continue;
		}
		AppendDescriptorWrites( bindParmsList[ i ], context.bufferId, descriptorWrites );
	}

	if( descriptorWrites.size() > 0 ) {
		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );
	}
}