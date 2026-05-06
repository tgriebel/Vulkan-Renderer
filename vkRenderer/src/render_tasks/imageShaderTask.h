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
// A callback is used instead of inheritance for now since most image processes will be very similar
typedef void imageShaderFrameBeginCallback_t( ImageShaderTask* imageProcess );

struct imageShaderCreateInfo_t
{
	const char*			name;				// Debug name
	hdl_t				progHdl;			// Shaders
	shaderPermId_t		permSet;			// Shader permutations
	Image*				outputImage;		// Main render image
	Image*				outputImage1;		// MRT
	Image*				outputImage2;		// MRT
	RenderContext*		context;			// Render context
	ResourceContext*	resources;			// Resource context
	uint32_t			inputImages;		// Input images to sample
	uint32_t			inputCubeImages;	// Input cubemaps to sample
	uint32_t			mipLevel;			// Mip-level to work on
	uint32_t			layer;				// Image layer to work on
	uint32_t			passCount;			// Used for multi-pass shaders (e.g. Separable Gaussian)
	imageInfo_t*		createInfos;		// Inline created framebuffer images (useful for temp images)
	uint32_t			taskImageCount;		// Number of inline framebuffer images
	uint32_t			viewId;				// For pairing to a RenderView
	bool				clear;				// Clear the output before rendering
	bool				present;			// Present output after this shader runs

	imageShaderFrameBeginCallback_t* callback;
};

class ImageShaderTask : public GpuTask
{
public:
	struct constants_t
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
	static const uint32_t	MaxBufferSizeInBytes = 256;
	static const uint32_t	ReservedConstantSizeInBytes = sizeof( constants_t );
	static const uint32_t	MaxConstantBlockSizeInBytes = ( MaxBufferSizeInBytes - ReservedConstantSizeInBytes );
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
	Image*					m_taskImages[ MaxOutputImages ] = {};
	uint32_t				m_taskImageCount;
	uint32_t				m_layer;
	uint32_t				m_mipLevel;
	uint32_t				m_viewId;
	uint32_t				m_passCount;
	uint32_t				m_image2dSlotCount;
	uint32_t				m_imageCubeSlotCount;

	imageShaderFrameBeginCallback_t* m_callback = nullptr;

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

	void				Execute( CommandContext& cmdContext );
};