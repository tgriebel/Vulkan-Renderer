#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

enum imageWritebackFlags_t : uint8_t
{
	WRITE_TO_DISK		= ( 1 << 0 ),
	CUBEMAP				= ( 1 << 1 ),
	PACKED_HDR			= ( 1 << 2 ),
	TRY_USE_API_COMMAND	= ( 1 << 3 ),
	SCREENSHOT			= ( 1 << 4 ),
};
DEFINE_ENUM_OPERATORS( imageWritebackFlags_t, uint8_t )


struct imageWriteBackCreateInfo_t
{
	const char* name;
	const char* fileName;
	Image* img;
	ImageView* imgCube[ 6 ];
	RenderContext* context;
	ResourceContext* resources;
	imageWritebackFlags_t	flags;
};


class ImageWritebackTask : public GpuTask
{
private:
	ImageArray				m_imageArray;
	RenderContext*			m_context;
	ResourceContext*		m_resources;
	GpuBuffer				m_writebackBuffer;
	GpuBuffer				m_resourceBuffer;
	ShaderBindParms*		m_parms;
	std::string				m_fileName;
	std::string				m_name;
	imageWritebackFlags_t	m_flags;
	bool					m_hasWriteback;

	void Init();
	void Shutdown();

public:

	ImageWritebackTask( const imageWriteBackCreateInfo_t& info );

	~ImageWritebackTask()
	{
		Shutdown();
	}

	void Resize() {}

	void FrameBegin();
	void FrameEnd();

	void Execute( CommandContext& context ) override;
};
