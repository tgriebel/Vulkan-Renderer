#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

class MipImageTask : public GpuTask
{
private:
	struct constants_t
	{
		vec4f		dimensions;
		uint32_t	pad0;
		uint32_t	pad1;
		uint32_t	pad2;
		uint32_t	pad3;
	};

	static const uint32_t	MaxBufferSizeInBytes		= 256;
	static const uint32_t	ReservedConstantSizeInBytes = sizeof( constants_t );
	static const uint32_t	MaxConstantBlockSizeInBytes = ( MaxBufferSizeInBytes - ReservedConstantSizeInBytes );
	static const uint32_t	MaxMipMaps = 16;

	Image*						m_image;
	downSampleMode_t			m_mode;
	std::string					m_dbgName;
	RenderContext*				m_context;
	ResourceContext*			m_resources;
	Image						m_tempImage;
	GpuBuffer					m_buffer;
	ImageView					m_imgViews[ MaxMipMaps ];
	DrawPass*					m_passes[ MaxMipMaps ];
	FrameBuffer					m_frameBuffers[ MaxMipMaps ];
	GpuBufferView				m_bufferViews[ MaxMipMaps ];
	uint32_t					m_mipLevels;
	uint32_t					m_layer;
	bool						m_firstFrame;

	void Init( const mipProcessCreateInfo_t& info );
	void Shutdown();

public:

	MipImageTask( const mipProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void		FrameBegin();
	void		FrameEnd();
	void		Resize();
	std::string	AsString() const;

	uint32_t	GetMipCount() const;
	bool		SetSourceImageForLevel( const uint32_t mipLevel, Image* img );
	bool		SetConstantsForLevel( const uint32_t mipLevel, const void* dataBlock, const uint32_t sizeInBytes );

	void		Execute( CommandContext& context ) override;

	~MipImageTask() {
		Shutdown();
	}
};