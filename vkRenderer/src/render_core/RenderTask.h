#pragma once

#include <queue>
#include <GfxCore/asset_types/material.h>
#include "../render_core/GpuSync.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

class CommandContext;
class GfxContext;
class RenderView;
class RenderContext;
class ResourceContext;
class Image;
struct ComputeState;

enum downSampleMode_t : uint32_t;

struct mipProcessCreateInfo_t
{
	const char*			name;
	Image*				img;
	uint32_t			layer;
	downSampleMode_t	mode;
	RenderContext*		context;
	ResourceContext*	resources;
};

struct imageWriteBackCreateInfo_t
{
	const char*			name;
	const char*			fileName;
	Image*				img;
	ImageView*			imgCube[6];
	RenderContext*		context;
	ResourceContext*	resources;
	bool				writeToDiskOnFrameEnd;
	bool				screenshot;
	bool				cubemap;
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

enum gpuImageStateFlags_t : uint8_t;

class GpuTask
{
public:
	virtual void	FrameBegin() = 0;
	virtual void	FrameEnd() = 0;
	virtual void	Resize() = 0;
	virtual void	Execute( CommandContext& context ) = 0;

	virtual ~GpuTask() {};
};


class RenderTask : public GpuTask
{
private:
	RenderView*		renderView;
	drawPass_t		beginPass;
	drawPass_t		endPass;
	GpuSemaphore	finishedSemaphore;

	void Init( RenderView* view, drawPass_t begin, drawPass_t end );
	void Shutdown();
	void RenderViewSurfaces( GfxContext* context );

public:
	RenderTask()
	{
		Init( nullptr, DRAWPASS_COUNT, DRAWPASS_COUNT );
	}

	RenderTask( RenderView* view, drawPass_t begin, drawPass_t end )
	{
		Init( view, begin, end );
	}

	~RenderTask()
	{
		Shutdown();
	}

	void Resize() {}

	void FrameBegin();
	void FrameEnd();

	void Execute( CommandContext& context ) override;
};


class ComputeTask : public GpuTask
{
private:
	ComputeState*		m_state;
	hdl_t				m_progHdl;

public:
	ComputeTask( const char* csName, ComputeState* state );

	void Resize() {}

	void FrameBegin();
	void FrameEnd();

	void Execute( CommandContext& context ) override;
	~ComputeTask()
	{}
};


class TransitionImageTask : public GpuTask
{
private:
	Image*					m_img;
	gpuImageStateFlags_t	m_srcState;
	gpuImageStateFlags_t	m_dstState;

public:

	TransitionImageTask( Image* img, const gpuImageStateFlags_t srcState, const gpuImageStateFlags_t dstState )
	{
		m_img = img;
		m_srcState = srcState;
		m_dstState = dstState;
	}

	void Resize() {}

	void FrameBegin() {}
	void FrameEnd() {}

	void Execute( CommandContext& context ) override;
	~TransitionImageTask()
	{}
};


class CopyImageTask : public GpuTask
{
private:
	Image*	m_src;
	Image*	m_dst;

public:

	CopyImageTask( Image* src, Image* dst )
	{
		m_src = src;
		m_dst = dst;
	}

	void Resize() {}

	void FrameBegin() {}
	void FrameEnd() {}

	void Execute( CommandContext& context ) override;
	~CopyImageTask()
	{}
};


class ImageWritebackTask : public GpuTask
{
private:
	ImageArray			m_imageArray;
	RenderContext*		m_context;
	ResourceContext*	m_resources;
	GpuBuffer			m_writebackBuffer;
	GpuBuffer			m_resourceBuffer;
	ShaderBindParms*	m_parms;
	std::string			m_fileName;
	std::string			m_name;
	bool				m_writeToDiskOnFrameEnd;
	bool				m_screenshot;

	void Init();
	void Shutdown();

public:

	ImageWritebackTask( const imageWriteBackCreateInfo_t& info )
	{	
		if( info.img != nullptr )
		{
			m_imageArray.Resize( 1 );
			m_imageArray[ 0 ] = info.img;
		}
		else if ( info.imgCube != nullptr )
		{
			m_imageArray.Resize( 6 );
			for( uint32_t i = 0; i < 6; ++i )
			{
				// Sort the slices so serialization is ordered 
				const uint32_t reorderIx = info.imgCube[ i ]->subResourceView.baseArray;
				m_imageArray[ reorderIx ] = info.imgCube[ i ];
			}
		}
		else
		{
			assert( 0 );
		}
		m_context = info.context;
		m_resources = info.resources;
		m_fileName = info.fileName;
		m_name = info.name;
		m_writeToDiskOnFrameEnd = info.writeToDiskOnFrameEnd;
		m_screenshot = info.screenshot;
		Init();
	}

	void Resize() {}

	void FrameBegin();
	void FrameEnd();

	void Execute( CommandContext& context ) override;
	~ImageWritebackTask()
	{
		Shutdown();
	}
};


class MipImageTask : public GpuTask
{
private:
	static const uint32_t	MaxBufferSizeInBytes = 256;
	static const uint32_t	ReservedConstantSizeInBytes = 16;
	static const uint32_t	MaxConstantBlockSizeInBytes = 240;

	Image*						m_image;
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


class RenderSchedule
{
private:
	std::vector< GpuTask* >	tasks;
	uint32_t				currentTask;
	
public:

	RenderSchedule() : currentTask( 0 )
	{}

	uint32_t	PendingTasks() const;
	void		Clear();
	void		Queue( GpuTask* task );
	void		FrameBegin();
	void		FrameEnd();
	void		IssueNext( CommandContext& context );
};