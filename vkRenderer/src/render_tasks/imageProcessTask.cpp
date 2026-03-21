#include "imageProcessTask.h"

#include <SysCore/systemUtils.h>

#include "../render_binding/gpuResources.h"
#include "../render_binding/bindings.h"


std::string ImageProcessTask::AsString() const
{
	std::stringstream ss;
	ss << "<ImageProcessTask: " << m_dbgName << ">";
	return ss.str();
}


void ImageProcessTask::Init( const imageProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "MipImageTaskInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	assert( ( info.taskImageCount == 0 ) || ( info.taskImageCount == 1 ) );

	m_dbgName = info.name;
	m_image = info.outputImage;
	m_context = info.context;
	m_resources = info.resources;
	m_taskImageCount = info.taskImageCount;

	if ( m_taskImageCount > 0 )
	{
		assert( m_image == nullptr );

		m_image = new Image(
				info.createInfos[ 0 ],
				nullptr,
				new GpuImage( "ImageProcessImage", info.createInfos[ 0 ], GPU_IMAGE_RW, m_context->frameBufferMemory, resourceLifeTime_t::TASK )
			);
	}

	assert( ( m_image->info.type == IMAGE_TYPE_2D ) || ( m_image->info.type == IMAGE_TYPE_CUBE ) );

	m_context->scratchMemory.AdjustOffset( 0, 0 );

	m_cubeMip = ( m_image->info.type == IMAGE_TYPE_CUBE );
	m_layers = m_cubeMip ? 6 : 1;

	m_requestedMipCount = info.mipCount;
	m_baseMip = info.baseMip;
	m_mipLevels = ( m_requestedMipCount == 0 ) ? m_image->info.mipLevels : m_requestedMipCount;
	// Clamp [1, mipLevels]
	m_mipLevels = Clamp( m_mipLevels - m_baseMip, m_baseMip + 1, m_image->info.mipLevels );

	m_progHdl = AssetLibGpuProgram::Handle( info.progName );
	m_multiPass = info.multiPass;
	m_useApi = ( m_progHdl == INVALID_HDL ) || info.useAPI;
	m_progressiveSampling = info.progressiveSampling;
	m_processLowToHigh = info.upsampleProcess;
	m_seedFromFirstResourceImageLastMIP = info.seedFromFirstResourceImageLastMIP;

	// With progressive sampling, the image processes chains MIP views from the same image resource
	// Input and output images are *technically* the same, but views resolve this conflict
	// Since the views refer to subresources
	const bool createViewForFirstSampleImage = m_progressiveSampling;

	// Images that are attached for sampling at all levels
	for ( uint32_t imageIx = 0; imageIx < MaxImageProcessSampleImages; ++imageIx )
	{
		if( info.sampleImages[ imageIx ] == nullptr ) {
			continue;
		}

		if( info.sampleImages[ imageIx ]->info.type == IMAGE_TYPE_2D )
		{
			m_resourceImages2d[ m_sample2dCount ] = info.sampleImages[ imageIx ];
			++m_sample2dCount;
		}

		if ( info.sampleImages[ imageIx ]->info.type == IMAGE_TYPE_CUBE )
		{
			m_resourceCubeImages[ m_sampleCubeCount ] = info.sampleImages[ imageIx ];
			++m_sampleCubeCount;
		}
	}

	assert( m_progressiveSampling || ( m_sample2dCount > 0 ) || ( m_sampleCubeCount > 0 ) ); // Need some source for pixels to downsample

	// Finished because shader resources aren't needed
	if( m_useApi ) {
		return;
	}

	// Set-up shader resources
	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		if( m_cubeMip )
		{
			Camera camera = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
			camera.SetFov( Radians( 90.0f ) );
			camera.SetAspectRatio( 1.0f );

			switch ( layerId )
			{
			case IMAGE_CUBE_FACE_X_POS:	camera.Pan( 0.0f * PI );	break;
			case IMAGE_CUBE_FACE_Y_POS:	camera.Pan( 0.5f * PI );	break;
			case IMAGE_CUBE_FACE_X_NEG:	camera.Pan( 1.0f * PI );	break;
			case IMAGE_CUBE_FACE_Y_NEG:	camera.Pan( 1.5f * PI );	break;
			case IMAGE_CUBE_FACE_Z_POS:	camera.Tilt( -0.5f * PI );	break;
			case IMAGE_CUBE_FACE_Z_NEG:	camera.Tilt( 0.5f * PI );	break;
			}

			mat4x4f& viewMatrix = m_viewMatrices[ layerId ];

			viewMatrix = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order
			viewMatrix[ 3 ][ 3 ] = 0.0f;
		}

		const uint32_t remappedLayerId = m_cubeMip ? vk_MapToGlslCubemapConvention( layerId ) : layerId;

		// Need a unique view so input/output subresources don't overlap
		if ( createViewForFirstSampleImage )
		{
			const uint32_t adjustedMipLevel = m_seedFromFirstResourceImageLastMIP ? ( m_mipLevels - m_baseMip - 1 ) : Max( 0, static_cast<int32_t>( m_baseMip ) - 1 );

			const Image* seedImage = m_seedFromFirstResourceImageLastMIP ? m_resourceImages2d[ 0 ] : m_image;

			imageSubResourceView_t sourceSubresource{};
			sourceSubresource.baseArray = remappedLayerId;
			sourceSubresource.arrayCount = 1;
			sourceSubresource.baseMip = adjustedMipLevel;
			sourceSubresource.mipLevels = 1;

			imageInfo_t imageInfo = seedImage->info;
			imageInfo.type = IMAGE_TYPE_2D;

			m_baseViews[ layerId ].Init( seedImage, imageInfo, sourceSubresource, resourceLifeTime_t::RESIZE );
		}

		// Image Process for writing each MIP
		for ( uint32_t mipLevel = m_baseMip; mipLevel < m_mipLevels; ++mipLevel )
		{
			const uint32_t adjustedMipLevel = m_processLowToHigh ? ( m_mipLevels - mipLevel - 1 ) : mipLevel;
			m_imgProcesses[ layerId ][ mipLevel ] = CreateImageShaderTask( layerId, adjustedMipLevel );
		}
	}
}


ImageShaderTask* ImageProcessTask::CreateImageShaderTask( const uint32_t layerId, const uint32_t mipLevel )
{
	const uint32_t remappedLayerId = m_cubeMip ? vk_MapToGlslCubemapConvention( layerId ) : layerId;

	imageShaderCreateInfo_t imgProcessInfo = {};
	imgProcessInfo.name = m_dbgName.c_str();
	imgProcessInfo.context = m_context;
	imgProcessInfo.resources = m_resources;
	imgProcessInfo.passCount = m_multiPass ? 2 : 1;
	imgProcessInfo.inputCubeImages = m_sampleCubeCount;
	imgProcessInfo.inputImages = m_sample2dCount + 1; // Index zero always includes the sample source (even when redundant)
	imgProcessInfo.layer = remappedLayerId;
	imgProcessInfo.outputImage = m_image;
	imgProcessInfo.progHdl = m_progHdl;

	// All but the first image need a framebuffer since they are being written to
	imgProcessInfo.mipLevel = mipLevel;

	ImageShaderTask* imageProcess = new ImageShaderTask( imgProcessInfo );

	if ( m_cubeMip ) {
		imageProcess->SetConstants( &m_viewMatrices[ layerId ], sizeof( mat4x4f ) );
	}
	return imageProcess;
}


void ImageProcessTask::FrameBegin()
{
	if ( m_useApi ) {
		return;
	}

	// Can lock to base view
	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		Image* sourceImage = nullptr;
		
		if( ( m_baseMip > 0 ) || m_seedFromFirstResourceImageLastMIP ) {
			sourceImage = &m_baseViews[ layerId ];
		} else {
			sourceImage = m_resourceImages2d[ 0 ];
		}

		// Chain mip level N as sample image for mip level N + 1
		for ( uint32_t mipLevel = m_baseMip; mipLevel < m_mipLevels; ++mipLevel )
		{
			ImageShaderTask* shaderTask = m_imgProcesses[ layerId ][ mipLevel ];

			if( m_resourceCubeImages[ 0 ] != nullptr )
			{
				shaderTask->SetSourceCubeImage( 0, m_resourceCubeImages[ 0 ] );
			}
			else
			{
				shaderTask->SetSourceImage( 0, sourceImage );

				for ( uint32_t imageIx = 0; imageIx < m_sample2dCount; ++imageIx ) {
					shaderTask->SetSourceImage( imageIx + 1, m_resourceImages2d[ imageIx ] );
				}
			}

			if( m_progressiveSampling ) {
				sourceImage = shaderTask->GetOutputImage();
			}
		}

		for ( uint32_t mipLevel = m_baseMip; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ layerId ][ mipLevel ]->FrameBegin();
		}
	}
}


void ImageProcessTask::FrameEnd()
{
	if ( m_useApi ) {
		return;
	}
	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		for ( uint32_t mipLevel = m_baseMip; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ layerId ][ mipLevel ]->FrameEnd();
		}
	}
}


void ImageProcessTask::Resize()
{
	const uint32_t newMipLevels = ( m_requestedMipCount == 0 ) ? m_image->info.mipLevels : Min( m_requestedMipCount, m_image->info.mipLevels );
	if ( m_useApi == false )
	{
		m_mipLevels = newMipLevels;
		return;
	}

	if( m_mipLevels > m_image->info.mipLevels )
	{
		for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
		{
			for ( uint32_t mipLevel = m_image->info.mipLevels; mipLevel < m_mipLevels; ++mipLevel )
			{
				delete m_imgProcesses[ layerId ][ mipLevel ];
				m_imgProcesses[ layerId ][ mipLevel ] = nullptr;
			}
		}
	}
	else if ( m_mipLevels < m_image->info.mipLevels )
	{
		for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
		{
			for ( uint32_t mipLevel = m_mipLevels; mipLevel < m_image->info.mipLevels; ++mipLevel )
			{
				m_imgProcesses[ layerId ][ mipLevel ] = CreateImageShaderTask( layerId, mipLevel );
			}
		}
	}
	m_mipLevels = newMipLevels;

	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		if ( m_progressiveSampling ) {
			m_baseViews[ layerId ].Resize();
		}

		for ( uint32_t mipLevel = m_baseMip; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ layerId ][ mipLevel ]->Resize();
		}
	}
}


void ImageProcessTask::Shutdown()
{
	if ( m_useApi ) {
		return;
	}

	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		for ( uint32_t mipLevel = m_baseMip; mipLevel < m_mipLevels; ++mipLevel ) {
			delete m_imgProcesses[ layerId ][ mipLevel ];
		}
		m_baseViews[ layerId ].Destroy();
	}
}


Image* ImageProcessTask::GetOutputImage()
{
	return m_image;
}


uint32_t ImageProcessTask::GetMipCount() const
{
	return m_mipLevels;
}


void ImageProcessTask::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( ColorPurple ) );

	if ( m_useApi )
	{
		Transition( &cmdContext, *m_image, GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_DST );
		GenerateMipmaps( &cmdContext, *m_image );
	}
	else
	{
		static const char* debugFaceNames[ 6 ] = { "X+", "X-", "Y+", "Y-", "Z+", "Z-" };

		for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
		{
			const uint32_t writeLayer = m_imgProcesses[ layerId ][ m_baseMip ]->GetOutputImage()->subResourceView.baseArray;

			if( m_cubeMip ) {
				cmdContext.MarkerBeginRegion( debugFaceNames[ writeLayer ], ColorToVector( ColorPurple ) );
			}

			uint32_t mipLevel = m_baseMip;
			for ( ; mipLevel < m_mipLevels; ++mipLevel ) {
				m_imgProcesses[ layerId ][ mipLevel ]->Execute( cmdContext );
			}

			if ( m_cubeMip ) {
				cmdContext.MarkerEndRegion();
			}
		}
	}

	cmdContext.MarkerEndRegion();
}
