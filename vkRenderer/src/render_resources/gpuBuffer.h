#pragma once
#include "../globals/common.h"
#include "../render_core/renderResource.h"
#include "../render_core/allocator.h"

enum class bufferType_t
{
	UNIFORM,
	STORAGE,
	VERTEX,
	INDEX,
	STAGING,
	READBACK,
	ACCELERATION_STRUCTURE,
	TLAS_INSTANCE_DATA,
	SHADER_BINDING_TABLE,
};


enum class bufferFlags_t : uint32_t
{
	NONE,
	RT_VISIBLE,
};
DEFINE_ENUM_OPERATORS( bufferFlags_t, uint32_t )

struct bufferCreateInfo_t
{
	const char*				name;
	swapBuffering_t			swapBuffering;
	uint32_t				elements;
	uint32_t				elementSizeBytes;
	bufferType_t			type;
	bufferFlags_t			flags;
	resourceLifeTime_t		lifetime;
	AllocatorMemory*		bufferMemory;
};


class GpuBufferView;

class GpuBuffer : public RenderResource
{
private:
	uint32_t		ClampId( const uint32_t bufferId ) const;
public:
	static uint64_t	GetAlignedSize( const uint64_t size, const uint64_t alignment );

	virtual void	SetPos( const uint64_t pos );
	virtual uint64_t GetMaxSize() const;

	uint64_t		GetSize() const;
	uint64_t		GetBaseOffset() const;
	uint64_t		GetElementSize() const;
	uint64_t		GetElementSizeAligned() const;
#ifdef USE_VULKAN
	VkBuffer&		VkObject();
	VkBuffer		GetVkObject() const;
	VkDeviceAddress	GetDeviceAddress() const;
#endif
	void			Create( const bufferCreateInfo_t info );
	void			Create( const char* name, const swapBuffering_t swapBuffering, const resourceLifeTime_t lifetime, const uint32_t elements, const uint32_t elementSizeBytes, const bufferType_t type, const bufferFlags_t flags = bufferFlags_t::NONE );
	void			Destroy();
	bool			VisibleToCpu() const;
	void			Allocate( const uint64_t size );
	void			CopyData( const void* data, const size_t sizeInBytes );
	void			CopyFrom( void* data, const size_t sizeInBytes ) const;
	void			Invalidate();	// Invalidates host cache for the current frame's buffer (no-op on coherent memory). Call after GPU writes, before CPU reads.
	void			Flush();		// Flushes host cache for the current frame's buffer (no-op on coherent memory). Call after CPU writes, before GPU reads.
	void*			Get() const;
	void*			GetPrevious() const;

	const char*		GetName() const;
	GpuBufferView	GetBufferView( const uint64_t baseElementIx, const uint64_t elementCount ) const;

protected:
	struct buffer_t
	{
		Allocation			alloc;
#ifdef USE_VULKAN
		VkBuffer			buffer;
#endif
		uint64_t			baseOffset;
		uint64_t			offset;
	};

	buffer_t			m_buffer[ MaxFrameStates ];
	uint32_t			m_bufferCount;
	uint64_t			m_end;
	uint64_t			m_elementSize;
	uint64_t			m_elementPadding;
	bufferType_t		m_type;
	swapBuffering_t		m_swapBuffering;
	const char*			m_name;

	friend class GpuBufferView;
};

class GpuBufferView : public GpuBuffer
{
private:
	using GpuBuffer::Create;
	using GpuBuffer::Destroy;
};
