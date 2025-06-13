#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

class MipImageTask : public GpuTask
{
private:
	static const uint32_t	MaxBufferSizeInBytes		= 256;
	static const uint32_t	ReservedConstantSizeInBytes	= 16;
	static const uint32_t	MaxConstantBlockSizeInBytes	= 240;

	Image* m_image;
	downSampleMode_t			m_mode;
	std::string					m_dbgName;
	RenderContext*				m_context;
	ResourceContext*			m_resources;
	Image						m_tempImage;
	GpuBuffer					m_buffer;
	std::vector<ImageView>		m_imgViews;
	std::vector<DrawPass*>		m_passes;
	std::vector<FrameBuffer>	m_frameBuffers;
	std::vector<GpuBufferView>	m_bufferViews;
	uint32_t					m_mipLevels;
	bool						m_firstFrame;

	void Init( const mipProcessCreateInfo_t& info );
	void Shutdown();

public:

	MipImageTask( const mipProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void		Resize() { assert( 0 ); } // support

	void		FrameBegin();
	void		FrameEnd();

	uint32_t	GetMipCount() const;
	bool		SetSourceImageForLevel( const uint32_t mipLevel, Image* img );
	bool		SetConstantsForLevel( const uint32_t mipLevel, const void* dataBlock, const uint32_t sizeInBytes );

	void		Execute( CommandContext& context ) override;

	~MipImageTask() {
		Shutdown();
	}
};