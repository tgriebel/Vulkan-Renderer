#pragma once

#include <cstdint>
#include <functional>

#include "asset.h"
#include "../render_core/renderResource.h"

class GpuImage;

enum gpuImageStateFlags_t : uint8_t;

enum imageType_t : uint8_t
{
	IMAGE_TYPE_UNKNOWN,
	IMAGE_TYPE_2D,
	IMAGE_TYPE_2D_ARRAY,
	IMAGE_TYPE_3D,
	IMAGE_TYPE_3D_ARRAY,
	IMAGE_TYPE_CUBE,
	IMAGE_TYPE_CUBE_ARRAY,
	IMAGE_TYPE_DEPTH,
	IMAGE_TYPE_STENCIL,
	IMAGE_TYPE_DEPTH_STENCIL,
};


enum imageAspectFlags_t : uint8_t
{
	IMAGE_ASPECT_NONE			= 0,
	IMAGE_ASPECT_COLOR_FLAG		= ( 1 << 0 ),
	IMAGE_ASPECT_DEPTH_FLAG		= ( 1 << 1 ),
	IMAGE_ASPECT_STENCIL_FLAG	= ( 1 << 2 ),
	IMAGE_ASPECT_PLANE0			= ( 1 << 3 ),
	IMAGE_ASPECT_PLANE1			= ( 1 << 4 ),
	IMAGE_ASPECT_PLANE2			= ( 1 << 5 ),
	IMAGE_ASPECT_ALL			= ( 1 << 6 ) - 1
};


enum imageTiling_t : uint8_t
{
	IMAGE_TILING_LINEAR,
	IMAGE_TILING_MORTON,
};


enum imageFmt_t : uint8_t
{
	IMAGE_FMT_UNKNOWN,
	IMAGE_FMT_R_8,
	IMAGE_FMT_R_16,
	IMAGE_FMT_R_32,
	IMAGE_FMT_D_16,
	IMAGE_FMT_D24S8,
	IMAGE_FMT_D_32,
	IMAGE_FMT_D_32_S8,
	IMAGE_FMT_RGB_8,
	IMAGE_FMT_RGBA_8,
	IMAGE_FMT_RGBA_8_UNORM, // Unsigned
	IMAGE_FMT_ABGR_8,
	IMAGE_FMT_BGR_8,
	IMAGE_FMT_BGRA_8,
	IMAGE_FMT_RG_32,
	IMAGE_FMT_RGB_16,
	IMAGE_FMT_RGBA_16,
	IMAGE_FMT_RGB_32,
	IMAGE_FMT_RGBA_32,
	IMAGE_FMT_R11G11B10_US, // Unsigned
};


enum imageSamples_t : uint8_t
{
	IMAGE_SMP_1 = (1 << 0),
	IMAGE_SMP_2 = ( 1 << 1 ),
	IMAGE_SMP_4 = ( 1 << 2 ),
	IMAGE_SMP_8 = ( 1 << 3 ),
	IMAGE_SMP_16 = ( 1 << 4 ),
	IMAGE_SMP_32 = ( 1 << 5 ),
	IMAGE_SMP_64 = ( 1 << 6 ),
};


struct imageInfo_t
{
	uint32_t				width;
	uint32_t				height;
	uint32_t				channels;
	uint32_t				mipLevels;
	uint32_t				layers;
	imageSamples_t			subsamples;
	imageType_t				type;
	imageFmt_t				fmt;
	imageAspectFlags_t		aspect;
	imageTiling_t			tiling;
	bool					unused; // Deprecated v2
};


enum imageCubeFace : uint8_t
{
	IMAGE_CUBE_FACE_X_POS,
	IMAGE_CUBE_FACE_X_NEG,
	IMAGE_CUBE_FACE_Y_POS,
	IMAGE_CUBE_FACE_Y_NEG,
	IMAGE_CUBE_FACE_Z_POS,
	IMAGE_CUBE_FACE_Z_NEG,
};


struct imageSubResourceView_t
{
	uint32_t baseMip;
	uint32_t mipLevels;
	uint32_t baseArray;
	uint32_t arrayCount;
};


enum samplerAddress_t
{
	SAMPLER_ADDRESS_WRAP = 0,
	SAMPLER_ADDRESS_CLAMP_EDGE = 1,
	SAMPLER_ADDRESS_CLAMP_BORDER = 2,
	SAMPLER_ADDRESS_MODES,
};


enum samplerFilter_t
{
	SAMPLER_FILTER_NEAREST = 0,
	SAMPLER_FILTER_BILINEAR = 1,
	SAMPLER_FILTER_TRILINEAR = 2,
	SAMPLER_FILTER_MODES,
};


enum samplerBorderColor_t
{
	SAMPLER_BORDER_WHITE = 0,
	SAMPLER_BORDER_BLACK = 1,
};


struct samplerState_t
{
	samplerAddress_t		addrMode;
	samplerFilter_t			filter;
	samplerBorderColor_t	borderColor;
	float					maxAniso; // 0.0f is disabled
	float					minLod;
	float					maxLod;
	bool					borderTransparent;
	bool					borderColorIsFloat;
	bool					pcf;
};


inline uint32_t GetBppForFormat( const imageFmt_t format )
{
	switch( format )
	{
		case IMAGE_FMT_R_8:				return 1;
		case IMAGE_FMT_R_16:			return 2;
		case IMAGE_FMT_R_32:			return 4;
		case IMAGE_FMT_D_16:			return 2;
		case IMAGE_FMT_D24S8:			return 4;
		case IMAGE_FMT_D_32:			return 4;
		case IMAGE_FMT_D_32_S8:			return 8; // Padded
		case IMAGE_FMT_RGB_8:			return 3;
		case IMAGE_FMT_RGBA_8:			return 4;
		case IMAGE_FMT_RGBA_8_UNORM:	return 4;
		case IMAGE_FMT_ABGR_8:			return 4;
		case IMAGE_FMT_BGR_8:			return 3;
		case IMAGE_FMT_BGRA_8:			return 4;
		case IMAGE_FMT_RG_32:			return 8;
		case IMAGE_FMT_RGB_16:			return 6;
		case IMAGE_FMT_RGBA_16:			return 8;
		case IMAGE_FMT_R11G11B10_US:	return 4;

		default: return 4;
	}
}


inline uint32_t GetChannelsForFormat( const imageFmt_t format )
{
	switch( format )
	{
		case IMAGE_FMT_R_8:				return 1;
		case IMAGE_FMT_R_16:			return 1;
		case IMAGE_FMT_R_32:			return 1;
		case IMAGE_FMT_D_16:			return 1;
		case IMAGE_FMT_D24S8:			return 2;
		case IMAGE_FMT_D_32:			return 1;
		case IMAGE_FMT_D_32_S8:			return 2;
		case IMAGE_FMT_RGB_8:			return 3;
		case IMAGE_FMT_RGBA_8:			return 4;
		case IMAGE_FMT_RGBA_8_UNORM:	return 4;
		case IMAGE_FMT_ABGR_8:			return 4;
		case IMAGE_FMT_BGR_8:			return 3;
		case IMAGE_FMT_BGRA_8:			return 4;
		case IMAGE_FMT_RG_32:			return 2;
		case IMAGE_FMT_RGB_16:			return 3;
		case IMAGE_FMT_RGBA_16:			return 4;
		case IMAGE_FMT_R11G11B10_US:	return 3;

		default: return 4;
	}
}


inline bool operator==( const imageInfo_t& info0, const imageInfo_t& info1 )
{
	bool equal =
		( info0.width == info1.width ) &&
		( info0.height == info1.height ) &&
		( info0.channels == info1.channels ) &&
		( info0.mipLevels == info1.mipLevels ) &&
		( info0.layers == info1.layers ) &&
		( info0.subsamples == info1.subsamples ) &&
		( info0.type == info1.type ) &&
		( info0.fmt == info1.fmt ) &&
		( info0.aspect == info1.aspect ) &&
		( info0.tiling == info1.tiling );
	return equal;
}


inline bool operator!=( const imageInfo_t& info0, const imageInfo_t& info1 )
{
	return !( info0 == info1 );
}


inline imageInfo_t DefaultImage2dInfo( uint32_t w, uint32_t h )
{
	imageInfo_t info {};
	info.width = w;
	info.height = h;
	info.layers = 1;
	info.channels = 4;
	info.mipLevels = MipCount( w, h );
	info.subsamples = IMAGE_SMP_1;
	info.type = IMAGE_TYPE_2D;
	info.fmt = IMAGE_FMT_RGBA_8;
	info.aspect = IMAGE_ASPECT_COLOR_FLAG;
	info.tiling = IMAGE_TILING_MORTON;

	return info;
}


class Image : public RenderResource
{
public:
	using ResizeFn = std::function<imageInfo_t( uint32_t width, uint32_t height )>;

private:
	static const uint32_t Version = 2;
	ResizeFn m_resizeFn;

public:

	imageInfo_t				info;
	imageSubResourceView_t	subResourceView;
	bool					generateMips;

	// `cpuImage` is loaded from disk, passed as a pointer to avoid slow copies. It can be explicitly deleted once uploaded
	// Ownership for `cpuImage` needs to be clearer--whatever does the allocation should also delete
	ImageBufferInterface*	cpuImage; // Memory lifetime is not tied to the object for now
	GpuImage*				gpuImage; // Does not own memory

	Image()
	{
		info = DefaultImage2dInfo( 1, 1 );

		subResourceView.baseArray = 0;
		subResourceView.arrayCount = 1;
		subResourceView.baseMip = 0;
		subResourceView.mipLevels = 1;

		generateMips = true;

		cpuImage = nullptr;
		gpuImage = nullptr;
	}

	Image( const imageInfo_t& _info ) : Image( _info, nullptr ) {}

	Image( const imageInfo_t& _info, ImageBufferInterface* _cpuImage )
	{
		Create( _info, _cpuImage );
	}

	Image( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime )
	{
		Create( _info, _name, _flags, _lifetime );
	}

	~Image()
	{
		cpuImage = nullptr;
		gpuImage = nullptr;
	}

	void Create( const imageInfo_t& _info )
	{
		Create( _info, nullptr, 0u );
	}

	void Create( const imageInfo_t& _info, uint8_t* pixelBytes, const uint32_t byteCount );

	void Create( const imageInfo_t& _info, ImageBufferInterface* _cpuImage );

	void Create( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime );

	void Destroy() override;

	bool OnResize( const uint32_t w, const uint32_t h ) override;

	virtual bool IsView() const { return false; }

	void RegisterResize( ResizeFn fn ) { m_resizeFn = std::move( fn ); }
	static ResizeFn FullDimensionResizeFn( const imageInfo_t& info );

	void Serialize( Serializer* serializer );
};


class ImageLoader : public LoadHandler<Image>
{
private:
	std::string		m_basePath;
	std::string		m_fileName;
	std::string		m_ext;
	bool			m_hdr;
	bool			m_cubemap;
	bool			m_linearColor;

	bool Load( Asset<Image>& texture );

public:
	ImageLoader() : m_cubemap( false ), m_hdr( false ), m_linearColor( false )
	{
	}

	ImageLoader( const std::string& path, const std::string& file, const bool linearColor ) : m_cubemap( false ), m_hdr( false ), m_linearColor( linearColor )
	{
		SetBasePath( path );
		SetTextureFile( file );
	}

	void SetBasePath( const std::string& path );
	void SetTextureFile( const std::string& file );
	void LoadAsCubemap( const bool isCubemap );
	void LoadAsLinear( const bool isLinear );
};

class BakedImageLoader : public LoadHandler<Image>
{
private:
	std::string m_basePath;
	std::string m_fileName;
	std::string m_ext;

	bool Load( Asset<Image>& texture );

public:
	BakedImageLoader() {}
	BakedImageLoader( const std::string& path, const std::string& ext )
	{
		SetBasePath( path );
		SetFileExt( ext );
	}

	void SetBasePath( const std::string& path );
	void SetFileExt( const std::string& ext );
};

using pImgLoader_t = Asset<Image>::loadHandlerPtr_t;
