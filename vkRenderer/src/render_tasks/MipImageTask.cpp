#include "MipImageTask.h"

#include <SysCore/systemUtils.h>

#include "../render_binding/gpuResources.h"
#include "../render_binding/bindings.h"

void MipImageTask::Init( const mipProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "MipImageTaskInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	assert( ( info.img->info.type == IMAGE_TYPE_2D ) || ( info.img->info.type == IMAGE_TYPE_CUBE ) );

	m_dbgName = info.name;
	m_image = info.img;
	m_mode = info.mode;
	m_context = info.context;
	m_resources = info.resources;

	m_context->scratchMemory.AdjustOffset( 0, 0 );

	m_mipLevels = m_image->info.mipLevels;

	m_passes.resize( m_mipLevels );
	m_imgViews.resize( m_mipLevels );
	m_frameBuffers.resize( m_mipLevels );
	m_bufferViews.resize( m_mipLevels );

	{
		m_tempImage.info = m_image->info;
		m_tempImage.info.mipLevels = 1;
		MipDimensions( 1, m_image->info.width, m_image->info.height, &m_tempImage.info.width, &m_tempImage.info.height );

		m_tempImage.Create(
			m_image->info,
			nullptr,
			new GpuImage( "tempMipImage", m_tempImage.info, GPU_IMAGE_RW | GPU_IMAGE_TRANSFER_SRC | GPU_IMAGE_TRANSFER_DST, m_context->scratchMemory, resourceLifeTime_t::TASK )
		);
	}

	// Create buffer
	m_buffer.Create( "Resource buffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::TASK, m_mipLevels, MaxBufferSizeInBytes, bufferType_t::UNIFORM, m_context->sharedMemory );

	// The last view is only needed to create a frame buffer 
	for ( uint32_t i = 0; i < m_mipLevels; ++i )
	{
		imageSubResourceView_t subView = {};
		subView.baseMip = i;
		subView.mipLevels = 1;
		subView.baseArray = info.layer;
		subView.arrayCount = 1;

		imageInfo_t viewInfo = m_image->info;
		viewInfo.type = IMAGE_TYPE_2D;

		m_imgViews[ i ].Init( *m_image, viewInfo, subView, resourceLifeTime_t::RESIZE );
	}

	// All but the first image need a framebuffer since they are being written to
	for ( uint32_t i = 1; i < m_mipLevels; ++i )
	{
		frameBufferCreateInfo_t info{};
		info.name = "MipDownsample";
		info.color0 = &m_tempImage;
		info.swapBuffering = swapBuffering_t::SINGLE_FRAME;

		m_frameBuffers[ i ].Create( info );

		imageProcessObject_t imageProcessParms{};

		const float w = float( m_imgViews[ i - 1 ].info.width );
		const float h = float( m_imgViews[ i - 1 ].info.height );
		imageProcessParms.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );
		assert( sizeof( imageProcessParms.dimensions ) <= ReservedConstantSizeInBytes );

		m_bufferViews[ i ] = m_buffer.GetView( i, 1 );

		m_bufferViews[ i ].SetPos( 0 );
		m_bufferViews[ i ].CopyData( &imageProcessParms, ReservedConstantSizeInBytes );

		m_passes[ i ] = new PostPass( &m_frameBuffers[ i ] );
		m_passes[ i ]->SetViewport( 0, 0, m_imgViews[ i ].info.width, m_imgViews[ i ].info.height );

		m_passes[ i ]->codeImages.Resize( 1 );
		m_passes[ i ]->codeImages[ 0 ] = &m_imgViews[ i - 1 ];

		m_passes[ i ]->parms = m_context->RegisterBindParm( bindset_imageProcess );
	}
	m_firstFrame = true;
}


void MipImageTask::FrameBegin()
{
	for ( uint32_t i = 1; i < m_mipLevels; ++i )
	{
		if ( m_image->info.type == IMAGE_TYPE_2D )
		{
			m_passes[ i ]->parms->Bind( bind_sourceImages, &m_passes[ i ]->codeImages );
			m_passes[ i ]->parms->Bind( bind_sourceCubeImages, rc.defaultImageCube );
		}
		else if ( m_image->info.type == IMAGE_TYPE_CUBE )
		{
			m_passes[ i ]->parms->Bind( bind_sourceImages, &rc.defaultImageArray );
			m_passes[ i ]->parms->Bind( bind_sourceCubeImages, m_passes[ i ]->codeCubeImages.Count() > 0 ? m_passes[ i ]->codeCubeImages[ 0 ] : rc.defaultImageCube );
		}
		m_passes[ i ]->parms->Bind( bind_imageStencil, &m_resources->stencilImageView );
		m_passes[ i ]->parms->Bind( bind_imageProcess, &m_bufferViews[ i ] );
	}
}


void MipImageTask::FrameEnd()
{

}


void MipImageTask::Shutdown()
{
	for ( uint32_t i = 0; i < m_mipLevels; i++ )
	{
		m_frameBuffers[ i ].Destroy();
		m_imgViews[ i ].Destroy();
		if ( m_passes[ i ] != nullptr ) {
			delete m_passes[ i ];
		}
	}
	delete m_tempImage.gpuImage;
}


uint32_t MipImageTask::GetMipCount() const
{
	return m_mipLevels;
}


bool MipImageTask::SetSourceImageForLevel( const uint32_t mipLevel, Image* img )
{
	if ( mipLevel > 0 && mipLevel >= GetMipCount() )
	{
		assert( 0 );
		return false;
	}

	if ( m_image->info.type == IMAGE_TYPE_2D )
	{
		m_passes[ mipLevel ]->codeImages.Resize( 1 );
		m_passes[ mipLevel ]->codeImages[ 0 ] = img;
	}
	else if ( m_image->info.type == IMAGE_TYPE_CUBE )
	{
		m_passes[ mipLevel ]->codeCubeImages.Resize( 1 );
		m_passes[ mipLevel ]->codeCubeImages[ 0 ] = img;
	}
	return true;
}


bool MipImageTask::SetConstantsForLevel( const uint32_t mipLevel, const void* dataBlock, const uint32_t sizeInBytes )
{
	if ( mipLevel > 0 && mipLevel >= GetMipCount() )
	{
		assert( 0 );
		return false;
	}
	assert( sizeInBytes <= MaxConstantBlockSizeInBytes );
	m_bufferViews[ mipLevel ].SetPos( ReservedConstantSizeInBytes );
	m_bufferViews[ mipLevel ].CopyData( dataBlock, Min( sizeInBytes, MaxConstantBlockSizeInBytes ) );
	return true;
}


void MipImageTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( ColorWhite ) );

	if ( m_mode == DOWNSAMPLE_LINEAR )
	{
		Transition( &context, *m_image, GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_DST );
		GenerateMipmaps( &context, *m_image );
	}
	else
	{
		if ( m_firstFrame ) {
			Transition( &context, m_tempImage, GPU_IMAGE_NONE, GPU_IMAGE_READ );
			m_firstFrame = false;
		}
		GenerateDownsampleMips( &context, m_imgViews, m_passes, m_mode );
	}

	context.MarkerEndRegion();
}
