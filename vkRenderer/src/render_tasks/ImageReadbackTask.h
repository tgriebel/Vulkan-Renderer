#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_resources/imageView.h"

enum imageReadbackFlags_t : uint8_t
{
	WRITE_TO_DISK		= ( 1 << 0 ),
	CUBEMAP				= ( 1 << 1 ),
	PACKED_HDR			= ( 1 << 2 ),
	TRY_USE_API_COMMAND	= ( 1 << 3 ),
	SCREENSHOT			= ( 1 << 4 ),
};
DEFINE_ENUM_OPERATORS( imageReadbackFlags_t, uint8_t )


struct imageReadBackCreateInfo_t
{
	const char*				name;
	const char*				fileName;
	Image*					img;
	RenderContext*			context;
	ResourceContext*		resources;
	imageReadbackFlags_t	flags;
};


class ImageReadbackTask : public GpuTask
{
private:
	Image*					m_readbackImage;	// Image to readback data into
	ImageArray				m_imageArray;		// For binding image views
	ImageView				m_cubeViews[ 6 ];	// Make a view for each face
	RenderContext*			m_context;
	ResourceContext*		m_resources;
	GpuBuffer				m_writebackBuffer;
	GpuBuffer				m_resourceBuffer;
	ShaderBindParms*		m_parms;
	std::string				m_fileName;
	std::string				m_name;
	imageReadbackFlags_t	m_flags;
	bool					m_hasWriteback;

	void Init( const imageReadBackCreateInfo_t& info );
	void Shutdown();

public:

	ImageReadbackTask( const imageReadBackCreateInfo_t& info )
	{
		Init( info );
	}

	~ImageReadbackTask()
	{
		Shutdown();
	}

	void Resize() {}

	void			FrameBegin();
	void			FrameEnd();
	std::string		AsString() const;

	void Execute( CommandContext& context ) override;
};
