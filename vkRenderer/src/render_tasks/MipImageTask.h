#pragma once

#include "../render_core/renderer.h"
#include "../render_state/frameBuffer.h"
#include "../render_binding/imageView.h"

enum downSampleMode_t : uint32_t;

struct mipProcessBlurInfo_t
{
	Image* sampleImage;
};


struct mipProcessCreateInfo_t
{
	const char*			name;
	Image*				img;
	uint32_t			layer;
	downSampleMode_t	mode;
	RenderContext*		context;
	ResourceContext*	resources;

	union
	{
		mipProcessBlurInfo_t	blurInfo;
	};
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
	struct constants_t
	{
		vec4f		dimensions;
		uint32_t	pad0;
		uint32_t	pad1;
		uint32_t	pad2;
		uint32_t	pad3;
	};

	static const uint32_t	MaxBufferSizeInBytes		= 256;
	static const uint32_t	ReservedConstantSizeInBytes = sizeof( constants_t );
	static const uint32_t	MaxConstantBlockSizeInBytes = ( MaxBufferSizeInBytes - ReservedConstantSizeInBytes );
	static const uint32_t	MaxMipMaps					= 16;

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
	bool						m_computeBaseMip;
	bool						m_useApi;

	void Init( const mipProcessCreateInfo_t& info );
	void Shutdown();

public:

	MipImageTask( const mipProcessCreateInfo_t& info )
	{
		Init( info );
	}

	void		FrameBegin();
	void		FrameEnd();
	void		Resize();
	std::string	AsString() const;

	uint32_t	GetMipCount() const;
	bool		SetSourceImageForLevel( const uint32_t mipLevel, Image* img );
	bool		SetConstantsForLevel( const uint32_t mipLevel, const void* dataBlock, const uint32_t sizeInBytes );

	void		Execute( CommandContext& context ) override;

	~MipImageTask() {
		Shutdown();
	}
};