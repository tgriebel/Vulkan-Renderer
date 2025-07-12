#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

struct mipProcessCreateInfo_t
{
	const char*			name;
	Image*				img;
	Image*				sampleImage;
	const char*			progName;
	RenderContext*		context;
	ResourceContext*	resources;

	uint32_t			baseMip;
	uint32_t			lastMip;

	bool				useAPI;
	bool				multiPass;
	bool				progressiveSampling;
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
	static const uint32_t MaxLayers = 6;

	hdl_t						m_progHdl;
	Image*						m_image;
	Image*						m_sampleImage;
	std::string					m_dbgName;
	RenderContext*				m_context;
	ResourceContext*			m_resources;

	mat4x4f						m_viewMatrices[ MaxLayers ];
	ImageProcess*				m_imgProcesses[ MaxLayers ][ MaxMipMaps ];
	ImageView					m_baseViews[ MaxLayers ];
	uint32_t					m_mipLevels;
	uint32_t					m_layers;
	uint32_t					m_baseMip;
	bool						m_firstFrame;
	bool						m_multiPass;
	bool						m_cubeMip;
	bool						m_progressiveSampling;
	bool						m_useApi;

	ImageProcess* CreateImageProcess( const uint32_t layerId, const uint32_t mipLevel );

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

	void		Execute( CommandContext& context ) override;

	~MipImageTask() {
		Shutdown();
	}
};