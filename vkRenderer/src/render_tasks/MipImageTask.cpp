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

	if ( info.mode == downSampleMode_t::DOWNSAMPLE_LINEAR )
	{
		m_progName = "DownSample";
		m_computeBaseMip = false;
		m_multiPass = false;
		m_useApi = false;
		m_sampleImage = m_image;
		m_progressiveSampling = true;
	}
	else if ( info.mode == downSampleMode_t::DOWNSAMPLE_GAUSSIAN )
	{
		m_progName = "SeparableGaussianBlur";
		m_computeBaseMip = true;
		m_multiPass = true;
		m_useApi = false;
		m_sampleImage = info.sampleImage;
		m_progressiveSampling = true;
	}
	else if( info.mode == downSampleMode_t::DOWNSAMPLE_SPECULAR_IBL )
	{
		m_progName = "preCalculatedSpecularIbl";
		m_computeBaseMip = true;
		m_multiPass = false;
		m_useApi = false;
		m_sampleImage = info.sampleImage;
		m_progressiveSampling = false;
	}
	else if ( info.mode == downSampleMode_t::DOWNSAMPLE_LINEAR_API )
	{
		m_progName = "MIP API";
		m_computeBaseMip = false;
		m_multiPass = false;
		m_useApi = true;
		m_sampleImage = nullptr;
		m_progressiveSampling = true;
	}

	if( m_useApi == false )
	{
		// Base View: Can be another image or the first MIP of the target image
		if ( m_progressiveSampling )
		{
			imageSubResourceView_t view{};
			view.baseArray = m_layer;
			view.arrayCount = 1;
			view.baseMip = 0;
			view.mipLevels = 1;

			imageInfo_t imageInfo = m_sampleImage->info;
			imageInfo.type = IMAGE_TYPE_2D;

			m_baseView.Init( m_sampleImage, imageInfo, view, resourceLifeTime_t::RESIZE );
		}

		// Image Process for writing each MIP
		{
			imageProcessCreateInfo_t imgProcessInfo = {};
			imgProcessInfo.name = info.name;
			imgProcessInfo.context = m_context;
			imgProcessInfo.resources = m_resources;
			imgProcessInfo.passCount = m_multiPass ? 2 : 1;
			imgProcessInfo.inputCubeImages = !m_progressiveSampling ? 1 : 0; // FIXME: quick hack b/c this happens to be true
			imgProcessInfo.inputImages = ( info.img->info.type == IMAGE_TYPE_2D ) && imgProcessInfo.inputCubeImages == 0;
			imgProcessInfo.layer = m_layer;
			imgProcessInfo.outputImage = m_image;
			imgProcessInfo.progHdl = AssetLibGpuProgram::Handle( m_progName );

			// All but the first image need a framebuffer since they are being written to
			for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel )
			{
				imgProcessInfo.mipLevel = mipLevel;

				m_imgProcesses[ mipLevel ] = new ImageProcess( imgProcessInfo );
			}
		}
	}
	m_firstFrame = true;
}


void MipImageTask::FrameBegin()
{
	if ( m_useApi ) {
		return;
	}

	// Can lock to base view
	Image* sourceImage = m_progressiveSampling  ? &m_baseView : m_sampleImage;

	// Chain mip level N as sample image for mip level N + 1
	for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel )
	{
		if( sourceImage->info.type == IMAGE_TYPE_CUBE )
		{
			assert( !m_progressiveSampling );
			m_imgProcesses[ mipLevel ]->SetSourceCubeImage( 0, sourceImage );
		}
		else
		{
			m_imgProcesses[ mipLevel ]->SetSourceImage( 0, sourceImage );
		}

		if( m_progressiveSampling ) {
			sourceImage = m_imgProcesses[ mipLevel ]->GetOutputImage();
		}
	}

	for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
		m_imgProcesses[ mipLevel ]->FrameBegin();
	}
}


void MipImageTask::FrameEnd()
{
	if ( m_useApi ) {
		return;
	}
	for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
		m_imgProcesses[ mipLevel ]->FrameEnd();
	}
}


void MipImageTask::Resize()
{
	m_mipLevels = m_image->info.mipLevels;

	m_baseView.Resize();

	for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
		m_imgProcesses[ mipLevel ]->Resize();
	}
}


void MipImageTask::Shutdown()
{
	if ( m_useApi == false )
	{
		for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
			delete m_imgProcesses[ mipLevel ];
		}
		m_baseView.Destroy();
	}
}


uint32_t MipImageTask::GetMipCount() const
{
	return m_mipLevels;
}


bool MipImageTask::SetConstants( const void* dataBlock, const uint32_t sizeInBytes )
{
	for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
		m_imgProcesses[ mipLevel ]->SetConstants( dataBlock, sizeInBytes );
	}
	return true;
}


void MipImageTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( ColorWhite ) );

	if ( m_useApi )
	{
		Transition( &context, *m_image, GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_DST );
		GenerateMipmaps( &context, *m_image );
	}
	else
	{
		uint32_t mipLevel = m_computeBaseMip ? 0 : 1;

		for ( ; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ mipLevel ]->Execute( context );
		}
	}

	context.MarkerEndRegion();
}
