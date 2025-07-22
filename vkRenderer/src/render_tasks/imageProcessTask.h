#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

struct imageProcessCreateInfo_t
{
	const char*			name;
	Image*				outputImage;
	Image*				sampleImage;
	const char*			progName;
	RenderContext*		context;
	ResourceContext*	resources;

	uint32_t			baseMip;
	uint32_t			taskImageCount;
	imageInfo_t*		createInfos;

	bool				useAPI;
	bool				singleLevel;
	bool				multiPass;
	bool				progressiveSampling;
};


class ImageProcessTask : public GpuTask
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
	ImageShaderTask*			m_imgProcesses[ MaxLayers ][ MaxMipMaps ];
	ImageView					m_baseViews[ MaxLayers ];
	uint32_t					m_mipLevels;
	uint32_t					m_layers;
	uint32_t					m_baseMip;
	uint32_t					m_taskImageCount;
	bool						m_firstFrame;
	bool						m_multiPass;
	bool						m_cubeMip;
	bool						m_progressiveSampling;
	bool						m_useApi;
	bool						m_singleLevel;

	ImageShaderTask* CreateImageShaderTask( const uint32_t layerId, const uint32_t mipLevel );

public:

	ImageProcessTask( const imageProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void			Init( const imageProcessCreateInfo_t& info );
	void			Shutdown();

	void			FrameBegin();
	void			FrameEnd();
	void			Resize();
	std::string		AsString() const;

	Image*			GetOutputImage();

	uint32_t		GetMipCount() const;

	void			Execute( CommandContext& context ) override;

	~ImageProcessTask() {
		Shutdown();
	}
};