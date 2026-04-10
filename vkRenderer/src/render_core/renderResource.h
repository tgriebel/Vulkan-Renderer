#pragma once
#include <assert.h>
#include <cstdint>
#include <vector>

class CommandContext;

enum class resourceLifeTime_t : uint8_t
{
	TASK,
	FRAME,
	RESIZE,
	REBOOT,
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
	SWAPCHAIN,
	IMAGE_VIEW,
	FRAMEBUFFER,
	BINDSET,
	IMAGE_SAMPLER,
	COUNT
};


class RenderResource
{
protected:
	resourceLifeTime_t	m_lifetime;
	resourceType_t		m_type;
	memoryRegion_t		m_resourceMemoryRegion;
	uint64_t			m_resourceByteCount;

public:
	void Create( const resourceType_t type, const resourceLifeTime_t lifetime );

	static std::vector<RenderResource*> GetResourceList( const resourceLifeTime_t lifetime );
	static void Cleanup( const resourceLifeTime_t lifetime );
	static void TransitionImages( CommandContext* cmdCommand, const resourceLifeTime_t lifetime );

	inline memoryRegion_t GetMemoryRegion() const
	{
		return m_resourceMemoryRegion;
	}

	inline uint64_t GetByteCount() const
	{
		return m_resourceByteCount;
	}

	virtual void Destroy() = 0;
};
