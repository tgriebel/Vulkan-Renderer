#pragma once

#include "../globals/common.h"
#include "../asset_types/gpuProgram.h"
#include "../draw_passes/drawpass.h"
#include "../draw_passes/postPass.h"
#include "../render_tasks/RenderTask.h"

class ShaderBindParms;
class RenderContext;
class ResourceContext;

class ImageShaderTask;

static const uint32_t MaxImageShaderSampleImages = 4;

struct imageShaderCreateInfo_t
{
	const char*			name;				// Debug name
	hdl_t				progHdl;			// Shaders
	shaderPermId_t		permSet;			// Optional. Shader permutations
	RenderContext*		context;			// Render context
	ResourceContext*	resources;			// Resource context
	Image*				outputImage;		// Main render image
	Image*				outputImage1;		// Optional. MRT
	Image*				outputImage2;		// Optional. MRT

	Image*				resourceImages[ MaxImageShaderSampleImages ]; // Optional. Input images to sample
	uint32_t			mipLevel;			// Optional. Mip-level to work on
	uint32_t			layer;				// Optional. Image layer to work on
	uint32_t			passCount;			// Optional. Used for multi-pass shaders (e.g. Separable Gaussian)
	imageInfo_t*		createInfos;		// Optional. Inline created framebuffer images (useful for temp images)
	uint32_t			taskImageCount;		// Optional. Number of inline framebuffer images
	uint32_t			viewId;				// Optional. Used to access RenderView constants (e.g. view matrix)
	const void*			constants;			// Optional. Custom shader constants
	uint32_t			constantsByteSize;	// Optional. Size in bytes of constants block
	bool				clear;				// Optional. Clear the output before rendering
	bool				present;			// Optional. Present output after this shader runs
};

class ImageShaderTask : public GpuTask
{
public:
	struct baseConstants_t
	{
		vec4f		dimensions;
		uint32_t	pass;
		uint32_t	previousImageId;
		uint32_t	level;
		uint32_t	layer;
		uint32_t	mipCount;
		uint32_t	layerCount;
		uint32_t	pad0;
		uint32_t	pad1;
	};

private:
	static const uint32_t	ReservedConstantSizeInBytes = sizeof( baseConstants_t );
	static const uint32_t	MaxConstantBlockSizeInBytes = ( MaxCustomConstantBytes - ReservedConstantSizeInBytes );
	static const uint32_t	MaxPasses = 2;
	static const uint32_t	MaxOutputImages = 3;

	Asset<GpuProgram>*		m_progAsset;
	shaderPermId_t			m_permSet;
	std::string				m_dbgName;
	renderPassTransition_t	m_transitionState;
	vec4f					m_clearColor;
	float					m_clearDepth;
	uint32_t				m_clearStencil;
	ResourceContext*		m_resources;
	RenderContext*			m_context;
	FrameBuffer				m_fb[ MaxPasses ];
	PostPass*				m_passes[ MaxPasses ];
	GpuBuffer				m_buffer[ MaxPasses ];
	ImageView*				m_outputImageViews[ MaxPasses ][ MaxOutputImages ] = {};
	Image*					m_image = nullptr;
	Image*					m_resourceImages2d[ MaxImageShaderSampleImages ] = {};
	Image*					m_resourceCubeImages[ MaxImageShaderSampleImages ] = {};
	Image*					m_taskImages[ MaxOutputImages ] = {};
	uint32_t				m_taskImageCount;
	uint32_t				m_layer;
	uint32_t				m_mipLevel;
	uint32_t				m_viewId;
	uint32_t				m_passCount;
	uint32_t				m_resource2dCount;
	uint32_t				m_resourceCubeCount;

public:
	ImageShaderTask() {}

	~ImageShaderTask()
	{
		Shutdown();
	}

	ImageShaderTask( const imageShaderCreateInfo_t& info )
	{
		Init( info );
	}

	void				Init( const imageShaderCreateInfo_t& info );
	void				Resize();
	void				Shutdown();

	void				FrameBegin();
	void				FrameEnd();
	std::string			AsString() const;

	ImageView*			GetOutputImage( const uint32_t outputImageIndex = 0 );

	void				SetSourceImage( const uint32_t slot, Image* image );
	void				SetSourceCubeImage( const uint32_t slot, Image* image );
	void				UpdateConstants( const void* dataBlock, const uint32_t sizeInBytes);

	void				Execute( CommandList& cmdContext );
};