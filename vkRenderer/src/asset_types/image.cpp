#include "image.h"
#include <syscore/systemUtils.h>
#include <syscore/serializer.h>
#include <syscore/common.h>
#include <gfxcore/io/serializeClasses.h>

#include "../scene/assetBaker.h"
#include "../io/serializeClasses.h"
#include "../io/io.h"
#include "../asset_types/assetLib.h"
#include "../render_core/gpuImage.h"


void Image::Create( const imageInfo_t& _info, uint8_t* pixelBytes, const uint32_t byteCount )
{
	{
		m_lifetime = resourceLifeTime_t::UNMANAGED;
		RenderResource::Create( resourceType_t::ASSET_IMAGE, m_lifetime );
	}

	info = _info;
	info.layers = ( _info.type == IMAGE_TYPE_CUBE ) ? 6 : _info.layers;

	subResourceView.arrayCount = info.layers;
	subResourceView.mipLevels = info.mipLevels;

	generateMips = true;

	assert( cpuImage == nullptr );

	imageBufferInfo_t bufferInfo{};
	bufferInfo.width = _info.width;
	bufferInfo.height = _info.height;
	bufferInfo.layers = _info.layers;
	bufferInfo.mipCount = _info.mipLevels;
	bufferInfo.data = pixelBytes;
	bufferInfo.dataByteCount = byteCount;

	switch ( info.fmt )
	{
		case IMAGE_FMT_R_8:
		{
			cpuImage = new ImageBuffer<uint8_t>( bufferInfo );
		} break;
		case IMAGE_FMT_D_16:
		case IMAGE_FMT_R_16:
		{
			cpuImage = new ImageBuffer<uint16_t>( bufferInfo );
		} break;
		case IMAGE_FMT_D_32:
		case IMAGE_FMT_R_32:
		{
			cpuImage = new ImageBuffer<float>( bufferInfo );
		} break;
		case IMAGE_FMT_RGB_8:
		{
			cpuImage = new ImageBuffer<rgb8_t>( bufferInfo );
		} break;
		case IMAGE_FMT_RGBA_8:
		case IMAGE_FMT_RGBA_8_UNORM:
		{
			cpuImage = new ImageBuffer<rgba8_t>( bufferInfo );
		} break;
		case IMAGE_FMT_RGB_16:
		{
			cpuImage = new ImageBuffer<rgb16_t>( bufferInfo );
		} break;
		case IMAGE_FMT_RGBA_16:
		{
			cpuImage = new ImageBuffer<rgba16_t>( bufferInfo );
		} break;
		default: assert( 0 );
	}
}


void Image::Create( const imageInfo_t& _info, ImageBufferInterface* _cpuImage, GpuImage* _gpuImage )
{
	if( ( _gpuImage != nullptr ) && HasFlags( _gpuImage->GetFlags(), gpuImageStateFlags_t::GPU_IMAGE_WRITE ) )
	{
		m_lifetime = _gpuImage->GetLifetime();
		RenderResource::Create( resourceType_t::FB_IMAGE, m_lifetime );

		if ( !m_resizeFn ) {
			RegisterResize( FullDimensionResizeFn( _info ) );
		}
	}
	else
	{
		m_lifetime = resourceLifeTime_t::UNMANAGED;
		RenderResource::Create( resourceType_t::ASSET_IMAGE, m_lifetime );
	}

	info = _info;
	info.layers = ( _info.type == IMAGE_TYPE_CUBE ) ? 6 : _info.layers;

	subResourceView.baseArray = 0;
	subResourceView.arrayCount = info.layers;
	subResourceView.baseMip = 0;
	subResourceView.mipLevels = info.mipLevels;

	generateMips = true;

	cpuImage = _cpuImage;
	gpuImage = _gpuImage;
}


void Image::Destroy()
{
	if ( cpuImage != nullptr )
	{
		delete cpuImage;
		cpuImage = nullptr;
	}
}


void Image::OnResize( uint32_t w, uint32_t h )
{
	if( m_resizeFn )
	{
		info = m_resizeFn( w, h );

		if( gpuImage != nullptr )
		{
			const std::string name = gpuImage->GetDebugName();
			const gpuImageStateFlags_t flags = gpuImage->GetFlags();
			const resourceLifeTime_t lifeTime = gpuImage->GetLifetime();

			gpuImage->Destroy();
			gpuImage->Create( name.c_str(), info, flags, lifeTime );
		}
	}
}


Image::ResizeFn Image::FullDimensionResizeFn( const imageInfo_t& info )
{
	return [ info ]( uint32_t w, uint32_t h ) -> imageInfo_t
	{
		imageInfo_t resized = info;
		resized.width = w;
		resized.height = h;

		// If the original had a mip-chain, recalculate the levels (in the default case)
		if( info.mipLevels > 1 ) {
			resized.mipLevels = MipCount( info.width, info.height );
		}
		return resized;
	};
}


void Image::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );

	SerializeStruct( s, info );

	if ( s->GetMode() == serializeMode_t::LOAD ) {
		Create( info );
	}

	if ( version == 2 ) {
		s->Next( generateMips );
	}

	cpuImage->Serialize( s );
}


bool ImageLoader::Load( Asset<Image>& imageAsset )
{
	Image& image = imageAsset.Get();

	sourceFile_t imgSource {};
	imgSource.path = m_basePath + m_fileName + "." + m_ext;
	imgSource.name = m_fileName;
	imgSource.isBakedAsset = ( m_ext == "img" ) || ( m_ext == "img.bin" );

	bakedAssetInfo_t info {};
	const bool loadedBaked = LoadBaked( imageAsset, info, imgSource, ".\\baked\\" + m_basePath, "img.bin" );
	if ( loadedBaked ) {
		return true;
	}

	if( m_ext == "img" ) {
		Serializer s( MB(32), serializeMode_t::LOAD );

		const std::string path = m_basePath + m_fileName + ".img";
		if ( SysCore::FileExists( path ) == false ) {
			return false;
		}
		s.ReadFile( path );

		image.Serialize(&s);

		return ( s.Status() == serializeStatus_t::OK );
	}

	if ( m_cubemap ) {
		return LoadCubeMapImage( ( m_basePath + m_fileName ).c_str(), m_ext.c_str(), image );
	} else {
		if( m_hdr ) {
			return LoadImageHDR( ( m_basePath + m_fileName + "." + m_ext ).c_str(), image );			
		} else {
			return LoadImage( ( m_basePath + m_fileName + "." + m_ext ).c_str(), m_linearColor, image );
		}
	}
}


void ImageLoader::SetBasePath( const std::string& path )
{
	m_basePath = path;
}


void ImageLoader::SetTextureFile( const std::string& file )
{
	SysCore::SplitFileName( file, m_fileName, m_ext );

	if( m_ext == "hdr" ) {
		m_hdr = true;
	}
}


void ImageLoader::LoadAsCubemap( const bool isCubemap )
{
	m_cubemap = isCubemap;
}


void ImageLoader::LoadAsLinear( const bool isLinear )
{
	m_linearColor = isLinear;
}


bool BakedImageLoader::Load( Asset<Image>& imageAsset )
{
	Image& image = imageAsset.Get();

	sourceFile_t imgSource {};
	imgSource.isBakedAsset = true;

	bakedAssetInfo_t info = {};
	const bool loadedBaked = LoadBaked( imageAsset, info, imgSource, m_basePath, m_ext );
	if ( loadedBaked ) {
		return true;
	}
	return false;
}


void BakedImageLoader::SetBasePath( const std::string& path )
{
	m_basePath = path;
}


void BakedImageLoader::SetFileExt( const std::string& ext )
{
	m_ext = ext;
}
