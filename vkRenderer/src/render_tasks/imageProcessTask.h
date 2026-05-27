#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_resources/imageView.h"

class ImageShaderTask;

static const uint32_t MaxImageProcessSampleImages = 4;

// Used for chained multi-pass processes such as MIP generation and downsampling
struct imageProcessCreateInfo_t
{
	const char*			name;
	Image*				sourceImage;									// Input image to start the process. Same as outputImage when null
	Image*				outputImage;									// Final output images from the process
	Image*				resourceImages[ MaxImageProcessSampleImages ];	// Images available to all shaders within the process
	const char*			progName;										// Shader used by process
	RenderContext*		context;
	ResourceContext*	resources;

	uint32_t			baseMip;										// Lowest MIP index to write to the output
	uint32_t			mipCount;										// Number of MIPs to process from the base MIP. 0 ignores this parameters and processes all levels from baseMIP
	uint32_t			taskImageCount;
	imageInfo_t*		createInfos;

	shaderPermId_t		permSet;										// Shader permutations to use for this shader. Default is empty

	const void*			constants;										// Optional custom shader constants (written after reserved IMAGE_CONSTANT_LAYOUT fields)
	uint32_t			constantsByteSize;								// Size in bytes of constants block

	uint32_t			viewId;											// For pairing to a RenderView
	bool				useAPI;											// Use the API for MIP generation
	bool				multiPass;										// Runs sequential shader passes (e.g. Separable Gaussian Blur)
	bool				progressiveSampling;							// Chains output to input (e.g. any MIP chain generation)
	bool				upsampleProcess;								// Process from lower res MIP to higher
};


class ImageProcessTask : public GpuTask
{
private:
	static const uint32_t MaxMipMaps = 16;
	static const uint32_t MaxLayers = 6;

	hdl_t						m_progHdl;
	shaderPermId_t				m_permSet;
	Image*						m_image;
	Image*						m_resourceImages2d[ MaxImageProcessSampleImages ];
	Image*						m_resourceCubeImages[ MaxImageProcessSampleImages ];
	std::string					m_dbgName;
	RenderContext*				m_context;
	ResourceContext*			m_resources;

	mat4x4f						m_viewMatrices[ MaxLayers ];
	ImageShaderTask*			m_imgProcesses[ MaxLayers ][ MaxMipMaps ] = {};
	ImageView					m_baseViews[ MaxLayers ];
	uint32_t					m_mipLevels;
	uint32_t					m_layers;
	uint32_t					m_baseMip;
	uint32_t					m_viewId;
	uint32_t					m_requestedMipCount;
	uint32_t					m_taskImageCount;
	uint32_t					m_resource2dCount;
	uint32_t					m_resourceCubeCount;
	bool						m_multiPass;
	bool						m_cubeMip;
	bool						m_useResource;
	bool						m_progressiveSampling;					// Chain output to inputs until finished
	bool						m_upsampleProcess;						// Used for upscaling
	bool						m_useApi;

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
	void			UpdateConstants();

	void			Execute( CommandList& context ) override;

	~ImageProcessTask() {
		Shutdown();
	}
};
