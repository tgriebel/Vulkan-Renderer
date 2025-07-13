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
	m_context = info.context;
	m_resources = info.resources;

	m_context->scratchMemory.AdjustOffset( 0, 0 );

	m_singleLevel = info.singleLevel;
	m_cubeMip = ( info.img->info.type == IMAGE_TYPE_CUBE );
	m_mipLevels = m_singleLevel ? 1 : m_image->info.mipLevels;
	m_layers = m_cubeMip ? 6 : 1;

	m_progHdl = AssetLibGpuProgram::Handle( info.progName );
	m_baseMip = info.baseMip;
	m_multiPass = info.multiPass;
	m_useApi = ( m_progHdl == INVALID_HDL ) || info.useAPI;
	m_sampleImage = ( info.sampleImage != nullptr ) ? info.sampleImage : m_image;
	m_progressiveSampling = info.progressiveSampling;

	if( m_useApi == false )
	{
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

				//for ( uint32_t j = 0; j < 4; ++j ) {
				//	for ( uint32_t i = 0; i < 4; ++i ) {
				//		viewMatrix[ j ][ i ] = i + j * 4;
				//	}
				//}

				viewMatrix = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order
				viewMatrix[ 3 ][ 3 ] = 0.0f;
			}

			const uint32_t remappedLayerId = m_cubeMip ? vk_MapToGlslCubemapConvention( layerId ) : layerId;

			// Base View: Can be another image or the first MIP of the target image
			if ( m_progressiveSampling )
			{
				imageSubResourceView_t view{};
				view.baseArray = remappedLayerId;
				view.arrayCount = 1;
				view.baseMip = 0;
				view.mipLevels = 1;

				imageInfo_t imageInfo = m_sampleImage->info;
				imageInfo.type = IMAGE_TYPE_2D;

				m_baseViews[ layerId ].Init( m_sampleImage, imageInfo, view, resourceLifeTime_t::RESIZE );
			}

			// Image Process for writing each MIP
			for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel )
			{
				m_imgProcesses[ layerId ][ mipLevel ] = CreateImageProcess( layerId, mipLevel );
			}
		}
	}
	m_firstFrame = true;
}


ImageProcess* MipImageTask::CreateImageProcess( const uint32_t layerId, const uint32_t mipLevel )
{
	const uint32_t remappedLayerId = m_cubeMip ? vk_MapToGlslCubemapConvention( layerId ) : layerId;

	imageProcessCreateInfo_t imgProcessInfo = {};
	imgProcessInfo.name = m_dbgName.c_str();
	imgProcessInfo.context = m_context;
	imgProcessInfo.resources = m_resources;
	imgProcessInfo.passCount = m_multiPass ? 2 : 1;
	imgProcessInfo.inputCubeImages = ( m_sampleImage->info.type == IMAGE_TYPE_CUBE ) ? 1 : 0;
	imgProcessInfo.inputImages = ( m_sampleImage->info.type == IMAGE_TYPE_2D ) ? 1 : 0;
	imgProcessInfo.layer = remappedLayerId;
	imgProcessInfo.outputImage = m_image;
	imgProcessInfo.progHdl = m_progHdl;

	// All but the first image need a framebuffer since they are being written to
	imgProcessInfo.mipLevel = mipLevel;

	ImageProcess* imageProcess = new ImageProcess( imgProcessInfo );

	if ( m_cubeMip ) {
		imageProcess->SetConstants( &m_viewMatrices[ layerId ], sizeof( mat4x4f ) );
	}
	return imageProcess;
}


void MipImageTask::FrameBegin()
{
	if ( m_useApi ) {
		return;
	}

	// Can lock to base view
	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		Image* sourceImage = m_progressiveSampling  ? &m_baseViews[ layerId ]: m_sampleImage;

		// Chain mip level N as sample image for mip level N + 1
		for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel )
		{
			if( sourceImage->info.type == IMAGE_TYPE_CUBE )
			{
				assert( !m_progressiveSampling );
				m_imgProcesses[ layerId ][ mipLevel ]->SetSourceCubeImage( 0, sourceImage );
			}
			else
			{
				m_imgProcesses[ layerId ][ mipLevel ]->SetSourceImage( 0, sourceImage );
			}

			if( m_progressiveSampling ) {
				sourceImage = m_imgProcesses[ layerId ][ mipLevel ]->GetOutputImage();
			}
		}

		for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ layerId ][ mipLevel ]->FrameBegin();
		}
	}
}


void MipImageTask::FrameEnd()
{
	if ( m_useApi ) {
		return;
	}
	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ layerId ][ mipLevel ]->FrameEnd();
		}
	}
}


void MipImageTask::Resize()
{
	if ( m_useApi == false )
	{
		m_mipLevels = m_singleLevel ? 1 : m_image->info.mipLevels;
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
				m_imgProcesses[ layerId ][ mipLevel ] = CreateImageProcess( layerId, mipLevel );
			}
		}
	}
	m_mipLevels = m_singleLevel ? 1 : m_image->info.mipLevels;

	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		if ( m_progressiveSampling ) {
			m_baseViews[ layerId ].Resize();
		}

		if ( m_progressiveSampling ) {
			m_baseViews[ layerId ].Resize();
		}

		for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
			m_imgProcesses[ layerId ][ mipLevel ]->Resize();
		}
	}
}


void MipImageTask::Shutdown()
{
	if ( m_useApi ) {
		return;
	}

	for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
	{
		for ( uint32_t mipLevel = 0; mipLevel < m_mipLevels; ++mipLevel ) {
			delete m_imgProcesses[ layerId ][ mipLevel ];
		}
		m_baseViews[ layerId ].Destroy();
	}
}


uint32_t MipImageTask::GetMipCount() const
{
	return m_mipLevels;
}


void MipImageTask::Execute( CommandContext& context )
{
	context.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( ColorPurple ) );

	if ( m_useApi )
	{
		Transition( &context, *m_image, GPU_IMAGE_READ, GPU_IMAGE_TRANSFER_DST );
		GenerateMipmaps( &context, *m_image );
	}
	else
	{
		static const char* faceNames[ 6 ] = { "X+", "X-", "Y+", "Y-", "Z+", "Z-" };

		for ( uint32_t layerId = 0; layerId < m_layers; ++layerId )
		{
			const uint32_t writeLayer = m_imgProcesses[ layerId ][ 0 ]->GetOutputImage()->subResourceView.baseArray;

			context.MarkerBeginRegion( faceNames[ writeLayer ], ColorToVector( ColorPurple ) );
			uint32_t mipLevel = m_baseMip;

			for ( ; mipLevel < m_mipLevels; ++mipLevel ) {
				m_imgProcesses[ layerId ][ mipLevel ]->Execute( context );
			}
			context.MarkerEndRegion();
		}
	}

	context.MarkerEndRegion();
}
