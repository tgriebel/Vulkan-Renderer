#pragma once

#include "common.h"
#include "../draw_passes/drawpass.h"
#include "../render_tasks/RenderTask.h"

class ShaderBindParms;
class RenderContext;
class ResourceContext;

class ImageProcess;
// A callback is used instead of inheritance for now since most image processes will be very similar
typedef void imageProcessFrameBeginCallback_t( ImageProcess* imageProcess );

struct imageProcessCreateInfo_t
{
	const char*			name;
	hdl_t				progHdl;
	Image*				image;
	RenderContext*		context;
	ResourceContext*	resources;
	uint32_t			inputImages;
	uint32_t			inputCubeImages;
	uint32_t			mipLevel;
	uint32_t			layer;
	uint32_t			passCount;
	bool				clear;
	bool				present;
	bool				resolve;

	imageProcessFrameBeginCallback_t * callback;
};

class ImageProcess : public GpuTask
{
private:
	struct constants_t
	{
		vec4f		dimensions;
		uint32_t	pass;
		uint32_t	previousImageId;
		uint32_t	pad0;
		uint32_t	pad1;
	};

	static const uint32_t	MaxBufferSizeInBytes = 256;
	static const uint32_t	ReservedConstantSizeInBytes = sizeof( constants_t );
	static const uint32_t	MaxConstantBlockSizeInBytes = ( MaxBufferSizeInBytes - ReservedConstantSizeInBytes );
	static const uint32_t	MaxPasses = 2;

	Asset<GpuProgram>*		m_progAsset;
	std::string				m_dbgName;
	renderPassTransition_t	m_transitionState;
	vec4f					m_clearColor;
	float					m_clearDepth;
	uint32_t				m_clearStencil;
	ResourceContext*		m_resources;
	RenderContext*			m_context;
	FrameBuffer				m_fb[ MaxPasses ];
	DrawPass*				m_passes[ MaxPasses ];
	GpuBuffer				m_buffer[ MaxPasses ];
	ImageView*				m_view[ MaxPasses ];;
	uint32_t				m_passCount;
	uint32_t				m_image2dSlotCount;
	uint32_t				m_imageCubeSlotCount;

	imageProcessFrameBeginCallback_t* m_callback = nullptr;

public:
	ImageProcess() {}

	~ImageProcess()
	{
		Shutdown();
	}

	ImageProcess( const imageProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void				Init( const imageProcessCreateInfo_t& info );
	void				Resize();
	void				Shutdown();

	void				FrameBegin();
	void				FrameEnd();
	std::string			AsString() const;

	ImageView*			GetWriteImage();

	void				SetSourceImage( const uint32_t slot, Image* image );
	void				SetSourceCubeImage( const uint32_t slot, Image* image );
	void				SetConstants( const void* dataBlock, const uint32_t sizeInBytes, const uint32_t passIndex = 0 );

	void				Execute( CommandContext& cmdContext );
};