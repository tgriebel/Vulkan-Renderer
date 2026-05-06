#pragma once
#include <assert.h>
#include <cstdint>
#include <vector>

class CommandList;

enum class resourceLifeTime_t : uint8_t
{
	TASK,
	FRAME,
	RESIZE,
	REBOOT,
	ASSET,
	UNMANAGED
};


enum memoryRegion_t
{
	UNKNOWN,
	LOCAL,
	SHARED,
};


enum class resourceType_t : uint8_t
{
	UNKNOWN,
	MEMORY,
	BUFFER,
	IMAGE,
	GPU_IMAGE,
	SWAPCHAIN,
	IMAGE_VIEW,
	FRAMEBUFFER,
	BINDSET,
	IMAGE_SAMPLER,
	COUNT
};


enum resourcePriority_t
{
	HIGHEST,
	MEDIUM,
	LOWEST,
};

class RenderResource
{
protected:
	resourceLifeTime_t	m_lifetime;
	resourceType_t		m_type;
	resourcePriority_t	m_priority;
	memoryRegion_t		m_resourceMemoryRegion;
	uint64_t			m_resourceByteCount;

public:
	void Create( const resourceType_t type, const resourceLifeTime_t lifetime );

	static std::vector<RenderResource*>& GetResourceList( const resourceLifeTime_t lifetime );
	static void Cleanup( const resourceLifeTime_t lifetime );
	static void ResizeResources( const uint32_t displayWidth, const uint32_t displayHeight );
	static void TransitionNewImages( CommandList* cmdList );

	inline resourceLifeTime_t GetLifetime() const
	{
		return m_lifetime;
	}

	inline resourceType_t GetType() const
	{
		return m_type;
	}

	inline resourcePriority_t GetPriority() const
	{
		return m_priority;
	}

	inline memoryRegion_t GetMemoryRegion() const
	{
		return m_resourceMemoryRegion;
	}

	inline uint64_t GetByteCount() const
	{
		return m_resourceByteCount;
	}

	// Returns if the resource was recreated on resize
	virtual bool OnResize( const uint32_t w, const uint32_t h )
	{
		return false;
	}
	virtual void Destroy() = 0;
};
