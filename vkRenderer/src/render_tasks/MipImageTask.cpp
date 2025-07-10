#include "MipImageTask.h"

#include <SysCore/systemUtils.h>

#include "../render_binding/gpuResources.h"
#include "../render_binding/bindings.h"


std::string MipImageTask::AsString() const
{
	std::stringstream ss;
	ss << "<MipImageTask: " << m_dbgName << ">";
	return ss.str();
}


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
	m_layer = info.layer;
	m_multiPass = ( info.mode == downSampleMode_t::DOWNSAMPLE_GAUSSIAN );

	// Image Process for writing each MIP
	{
		imageProcessCreateInfo_t imgProcessInfo = {};
		imgProcessInfo.name = info.name;
		imgProcessInfo.context = m_context;
		imgProcessInfo.resources = m_resources;
		imgProcessInfo.passCount = m_multiPass ? 2 : 1;
		imgProcessInfo.inputImages = 1;
		imgProcessInfo.image = m_image;

		char* progName = nullptr;
		switch ( info.mode )
		{
			case downSampleMode_t::DOWNSAMPLE_LINEAR:			progName = "DownSample";				break;
			case downSampleMode_t::DOWNSAMPLE_GAUSSIAN:			progName = "SeparableGaussianBlur";		break;
			case downSampleMode_t::DOWNSAMPLE_SPECULAR_IBL:		progName = "preCalculatedSpecularIbl";	break;
		}
		imgProcessInfo.progHdl = AssetLibGpuProgram::Handle( progName );

		imageSubResourceView_t view{};
		view.baseArray = m_layer;
		view.arrayCount = 1;
		view.baseMip = 0;
		view.mipLevels = 1;

		imageInfo_t imageInfo = m_image->info;
		imageInfo.type = IMAGE_TYPE_2D;

		m_baseView.Init( m_image, imageInfo, view, resourceLifeTime_t::RESIZE );

		Image* sourceImage = &m_baseView;

		// All but the first image need a framebuffer since they are being written to
		for ( uint32_t i = 1; i < m_mipLevels; ++i )
		{
			imgProcessInfo.mipLevel = i;

			m_imgProcesses[ i ] = new ImageProcess( imgProcessInfo );
			m_imgProcesses[ i ]->SetSourceImage( 0, sourceImage );

			sourceImage = m_imgProcesses[ i ]->GetOutputImage();
		}
	}
	m_firstFrame = true;
}


void MipImageTask::FrameBegin()
{
	for ( uint32_t i = 1; i < m_mipLevels; ++i ) {
		m_imgProcesses[ i ]->FrameBegin();
	}
}


void MipImageTask::FrameEnd()
{
	for ( uint32_t i = 1; i < m_mipLevels; ++i ) {
		m_imgProcesses[ i ]->FrameEnd();
	}
}


void MipImageTask::Resize()
{
	m_mipLevels = m_image->info.mipLevels;

	m_baseView.Resize();

	for ( uint32_t i = 1; i < m_mipLevels; ++i )
	{
		for ( uint32_t i = 1; i < m_mipLevels; ++i ) {
			m_imgProcesses[ i ]->Resize();
		}
	}
}


void MipImageTask::Shutdown()
{
	for ( uint32_t i = 0; i < m_mipLevels; i++ ) {
		delete m_imgProcesses[ i ];
	}
	m_baseView.Destroy();
}


uint32_t MipImageTask::GetMipCount() const
{
	return m_mipLevels;
}


bool MipImageTask::SetSourceImageForLevel( const uint32_t mipLevel, Image* img )
{
	// TODO: Delete this function

	if ( mipLevel > 0 && mipLevel >= GetMipCount() )
	{
		assert( 0 );
		return false;
	}

	if ( m_image->info.type == IMAGE_TYPE_2D )
	{
		m_imgProcesses[ mipLevel ]->SetSourceImage( 0, img );
	}
	else if ( m_image->info.type == IMAGE_TYPE_CUBE )
	{
		m_imgProcesses[ mipLevel ]->SetSourceCubeImage( 0, img );
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

	m_imgProcesses[ mipLevel ]->SetConstants( dataBlock, sizeInBytes );

	return true;
}


void MipImageTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( ColorWhite ) );

	if ( m_mode == DOWNSAMPLE_LINEAR_API )
	{
		Transition( &context, *m_image, GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_DST );
		GenerateMipmaps( &context, *m_image );
	}
	else
	{
		for ( uint32_t i = 1; i < m_mipLevels; ++i ) {
			m_imgProcesses[ i ]->Execute( context );
		}
	}

	context.MarkerEndRegion();
}
