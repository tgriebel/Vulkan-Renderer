#pragma once
#include <assert.h>
#include <cstdint>
#include <vector>

enum class resourceLifeTime_t : uint8_t
{
	TASK,
	FRAME,
	RESIZE,
	REBOOT,
	UNMANAGED
};


class RenderResource
{
protected:
	resourceLifeTime_t m_lifetime;

public:

	void Create( const resourceLifeTime_t lifetime );

	static std::vector<RenderResource*> GetResourceList( const resourceLifeTime_t lifetime );
	static void Cleanup( const resourceLifeTime_t lifetime );

	virtual void Destroy() = 0;
};