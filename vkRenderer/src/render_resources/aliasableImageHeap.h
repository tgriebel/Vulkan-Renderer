#pragma once

#include "../globals/common.h"
#include "../render_core/renderResource.h"
#include "../render_core/allocator.h"
#include "../asset_types/image.h"

// A fixed-size device-local memory block that multiple GpuImages can alias.
// Each aliased image claims a VkImage bound to this allocation — only one
// may be in use at a time, but they do not need to share a format.
//
// Sized at creation for the largest expected image (e.g. full-res RGBA_16).
// Because the heap does not resize, it must be large enough for the highest
// resolution that aliased images will ever be created at.  Typically created
// at REBOOT lifetime and sized for the maximum supported render resolution.

class AliasableImageHeap : public RenderResource
{
private:
#ifdef USE_VULKAN
	VmaAllocation	m_allocation = VK_NULL_HANDLE;
	VkDeviceSize	m_sizeBytes  = 0;
#endif

public:
	// Create the heap by querying memory requirements from a reference image
	// (refInfo describes the largest image that will ever alias this heap).
	// All aliased images must fit within the resulting allocation.
	void Create( const char* name, const imageInfo_t& refInfo, gpuImageStateFlags_t flags, resourceLifeTime_t lifetime );
	void Destroy() override;

	// The heap does not self-resize; callers must ensure the initial size covers
	// any resolution that aliased images may reach.
	bool OnResize( const uint32_t /*w*/, const uint32_t /*h*/ ) override { return false; }

#ifdef USE_VULKAN
	VmaAllocation GetAllocation() const { return m_allocation; }
	VkDeviceSize  GetSizeBytes()  const { return m_sizeBytes;  }
#endif
};
