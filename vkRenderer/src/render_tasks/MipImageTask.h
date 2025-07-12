#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

enum downSampleMode_t : uint32_t;


struct mipProcessCreateInfo_t
{
	const char*			name;
	Image*				img;
	Image*				sampleImage;
	uint32_t			layer;
	downSampleMode_t	mode;
	RenderContext*		context;
	ResourceContext*	resources;
};


union mipProcessParms_t
{
	struct downsample
	{
		uint32_t a;
		uint32_t b;
		uint32_t c;
		uint32_t d;
	};
};

class MipImageTask : public GpuTask
{
private:
	static const uint32_t MaxMipMaps = 16;

	Image*						m_image;
	Image*						m_sampleImage;
	const char*					m_progName;
	downSampleMode_t			m_mode;
	std::string					m_dbgName;
	RenderContext*				m_context;
	ResourceContext*			m_resources;

	ImageProcess*				m_imgProcesses[ MaxMipMaps ];
	ImageView					m_baseView;
	uint32_t					m_mipLevels;
	uint32_t					m_layer;
	bool						m_firstFrame;
	bool						m_multiPass;
	bool						m_progressiveSampling;
	bool						m_computeBaseMip;
	bool						m_useApi;

public:

	MipImageTask( const mipProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void		Init( const mipProcessCreateInfo_t& info );
	void		Shutdown();

	void		FrameBegin();
	void		FrameEnd();
	void		Resize();
	std::string	AsString() const;

	uint32_t	GetMipCount() const;
	bool		SetConstants( const void* dataBlock, const uint32_t sizeInBytes );

	void		Execute( CommandContext& context ) override;

	~MipImageTask() {
		Shutdown();
	}
};