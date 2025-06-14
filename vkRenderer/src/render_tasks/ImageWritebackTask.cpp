#include "ImageWritebackTask.h"

#include <SysCore/serializer.h>
#include <GfxCore/io/serializeClasses.h>

#include "../render_binding/gpuResources.h"
#include "../render_binding/bindings.h"

ImageWritebackTask::ImageWritebackTask( const imageWriteBackCreateInfo_t& info )
{
	if ( info.img != nullptr )
	{
		m_imageArray.Resize( 1 );
		m_imageArray[ 0 ] = info.img;
	}
	else if ( info.imgCube != nullptr )
	{
		m_imageArray.Resize( 6 );
		for ( uint32_t i = 0; i < 6; ++i )
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
	m_flags = info.flags;
	m_hasWriteback = false;

	Init();
}

void ImageWritebackTask::Init()
{
	struct writeBackParms_t
	{
		vec4f		dimensions;
	};

	writeBackParms_t writeBackParms {};

	const uint32_t maxBpp = sizeof( vec4f ); // Data from the readback is float due to buffer restrictions
	const uint32_t elementsCount = m_imageArray[ 0 ]->info.width * m_imageArray[ 0 ]->info.height * m_imageArray.Count();
	m_writebackBuffer.Create( "Writeback Buffer", swapBuffering_t::MULTI_FRAME, resourceLifeTime_t::TASK, elementsCount, maxBpp, bufferType_t::STORAGE, m_context->sharedMemory );
	m_resourceBuffer.Create( "Resource buffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::TASK, 1, sizeof( writeBackParms ), bufferType_t::UNIFORM, m_context->sharedMemory );

	writeBackParms.dimensions = vec4f( (float)m_imageArray[ 0 ]->info.width, (float)m_imageArray[ 0 ]->info.height, (float)m_imageArray.Count(), 0.0f );

	m_resourceBuffer.SetPos( 0 );
	m_resourceBuffer.CopyData( &writeBackParms, sizeof( writeBackParms ) );

	m_parms = m_context->RegisterBindParm( m_context->LookupBindSet( bindset_compute ) );
}


void ImageWritebackTask::FrameBegin()
{
	m_parms->Bind( bind_globalsBuffer, &m_resources->globalConstants );
	m_parms->Bind( bind_computeImage, &m_imageArray );
	m_parms->Bind( bind_computeParms, &m_resourceBuffer );
	m_parms->Bind( bind_computeWrite, &m_writebackBuffer );
}


void ImageWritebackTask::FrameEnd()
{
	if ( m_hasWriteback == false ) {
		return;
	}
	m_hasWriteback = false;

	FlushGPU();

	assert( m_writebackBuffer.VisibleToCpu() );

	// FIXME: Should writeback into the source image, not a separate copy
	// This is just used for file output and not general CPU readbacks currently
	Image img;
	img.info = m_imageArray[ 0 ]->info;
	img.info.type = IMAGE_TYPE_2D;
	img.info.fmt = IMAGE_FMT_RGBA_8;
	img.info.channels = 4;
	img.info.layers = 1;

	if ( HasFlags( m_flags, CUBEMAP ) )
	{
		img.info.type = IMAGE_TYPE_CUBE;
		img.info.layers = 6;
	}

	if ( HasFlags( m_flags, PACKED_HDR ) )
	{
		img.info.fmt = IMAGE_FMT_RGBA_16;
		img.cpuImage = new ImageBuffer<rgbaTupleh_t>();

		imageBufferInfo_t info{};
		info.width = img.info.width;
		info.height = img.info.height;
		info.layers = img.info.layers;
		info.mipCount = 1;
		info.bpp = sizeof( rgbaTupleh_t );

		img.cpuImage->Init( info );

		float* floatData = reinterpret_cast<float*>( m_writebackBuffer.Get() );
		rgbaTupleh_t* convertedData = reinterpret_cast<rgbaTupleh_t*>( img.cpuImage->Ptr() );

		const uint32_t bufferLength = img.cpuImage->GetPixelCount();
		for ( uint32_t i = 0; i < bufferLength; ++i )
		{
			rgbaTupleh_t rgba16;
			rgba16.r = PackFloat32( floatData[ 3 ] );
			rgba16.g = PackFloat32( floatData[ 2 ] );
			rgba16.b = PackFloat32( floatData[ 1 ] );
			rgba16.a = PackFloat32( floatData[ 0 ] );

			convertedData[ i ] = rgba16;
			floatData += 4;
		}
	}
	else
	{
		imageBufferInfo_t info{};
		info.width = img.info.width;
		info.height = img.info.height;
		info.layers = img.info.layers;
		info.mipCount = 1;
		info.bpp = sizeof( RGBA );

		img.cpuImage = new ImageBuffer<RGBA>(); // TODO: Merge Init() and constructor
		img.cpuImage->Init( info );

		float* floatData = reinterpret_cast<float*>( m_writebackBuffer.Get() );
		RGBA* convertedData = reinterpret_cast<RGBA*>( img.cpuImage->Ptr() );

		const uint32_t bufferLength = img.cpuImage->GetPixelCount();
		for ( uint32_t i = 0; i < bufferLength; ++i )
		{
			Color color = Color( floatData[ 3 ], floatData[ 2 ], floatData[ 1 ], floatData[ 0 ] );
			convertedData[ i ] = color.AsRGBA();
			floatData += 4;
		}
	}

	if ( HasFlags( m_flags, WRITE_TO_DISK ) )
	{
		if ( HasFlags( m_flags, SCREENSHOT ) )
		{
			WriteImage( ( ScreenshotPath + m_fileName ).c_str(), img );
		}
		else
		{
			Serializer* s = new Serializer( img.cpuImage->GetByteCount() + 1024, serializeMode_t::STORE );
			img.Serialize( s );
			s->WriteFile( TexturePath + CodeAssetPath + m_fileName );
			delete s;
		}
	}
}



void ImageWritebackTask::Execute( CommandContext& cmdContext )
{
	if ( HasFlags( m_flags, SCREENSHOT ) && g_imguiControls.captureScreenshot == false ) {
		return;
	}
	g_imguiControls.captureScreenshot = false;

	if ( HasFlags( m_flags, TRY_USE_API_COMMAND ) == false )
	{
		const uint32_t blockSize = 8;

		const Image* img = m_imageArray[ 0 ];
		const uint32_t w = img->info.width;
		const uint32_t h = img->info.height;
		const uint32_t layers = m_imageArray.Count();

		struct pushConstants_t
		{
			vec4f		dimensions;
			uint32_t	imageId;
			int32_t		lod;
			uint32_t	baseOffset;
		};

		pushConstants_t constants {};
		constants.dimensions = vec4f( (float)w, (float)h, (float)layers, 0.0f );
		constants.imageId = 0;
		constants.lod = 0;
		constants.baseOffset = 0;

		const hdl_t progHdl = AssetLibGpuProgram::Handle( "ImageWriteback" );
		cmdContext.Dispatch( progHdl, *m_parms, &constants, sizeof( pushConstants_t ),  w / blockSize + 1, h / blockSize + 1, layers / blockSize + 1 );
	}
	else
	{
		Transition( &cmdContext, *m_imageArray[ 0 ], GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_SRC );

		VkImageSubresourceLayers subLayers{};
		subLayers.aspectMask = vk_GetAspectFlags( m_imageArray[ 0 ]->info.aspect );
		subLayers.baseArrayLayer = 0;
		subLayers.layerCount = 1;
		subLayers.mipLevel = 0;

		VkBufferImageCopy copyParms{};
		copyParms.bufferOffset = 0;
		copyParms.imageExtent.width = m_imageArray[ 0 ]->info.width;
		copyParms.imageExtent.height = m_imageArray[ 0 ]->info.height;
		copyParms.imageExtent.depth = 1;
		copyParms.imageSubresource = subLayers;

		vkCmdCopyImageToBuffer( cmdContext.CommandBuffer(), m_imageArray[ 0 ]->gpuImage->GetVkImage( context.bufferId ), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_writebackBuffer.GetVkObject(), 1, &copyParms );

		Transition( &cmdContext, *m_imageArray[ 0 ], GPU_IMAGE_TRANSFER_SRC, GPU_IMAGE_READ );
	}
	m_hasWriteback = true;
}


void ImageWritebackTask::Shutdown()
{
}
