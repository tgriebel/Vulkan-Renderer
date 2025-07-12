#include "mipCubeProcess.h"
#include "MipImageTask.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"

extern AssetManager g_assets;

std::string MipCubeProcess::AsString() const
{
	std::stringstream ss;
	ss << "<MipCubeProcess: " << m_name << ">";
	return ss.str();
}


void MipCubeProcess::Init( const mipCubeProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "MipCubeProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	assert( ( info.img->info.layers == 6 ) && ( info.img->info.type == IMAGE_TYPE_CUBE ) );

	m_name = info.name;
	m_image = info.img;
	{
		mipProcessCreateInfo_t mipProcessInfo = {};
		mipProcessInfo.name = m_name.c_str();
		mipProcessInfo.sampleImage = info.sampleImage;
		mipProcessInfo.context = info.context;
		mipProcessInfo.resources = info.resources;
		mipProcessInfo.mode = info.mode;

		for ( uint32_t faceId = 0; faceId < 6; ++faceId )
		{
			const uint32_t layer = vk_MapToGlslCubemapConvention( faceId );

			imageSubResourceView_t view {};
			view.baseArray = layer;
			view.arrayCount = 1;
			view.baseMip = 0;
			view.mipLevels = 1;

			imageInfo_t imageInfo = info.img->info;
			imageInfo.type = IMAGE_TYPE_2D;

			m_outputFaceImage[ faceId ].Init( info.img, imageInfo, view, resourceLifeTime_t::RESIZE );

			mipProcessInfo.img = &m_outputFaceImage[ faceId ];
			mipProcessInfo.layer = layer;

			MipImageTask* mipProcess = new MipImageTask( mipProcessInfo );

			Camera camera = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
			camera.SetFov( Radians( 90.0f ) );
			camera.SetAspectRatio( 1.0f );	

			switch ( faceId )
			{
				case IMAGE_CUBE_FACE_X_POS:	camera.Pan( 0.0f * PI );	break;
				case IMAGE_CUBE_FACE_Y_POS:	camera.Pan( 0.5f * PI );	break;
				case IMAGE_CUBE_FACE_X_NEG:	camera.Pan( 1.0f * PI );	break;
				case IMAGE_CUBE_FACE_Y_NEG:	camera.Pan( 1.5f * PI );	break;
				case IMAGE_CUBE_FACE_Z_POS:	camera.Tilt( -0.5f * PI );	break;
				case IMAGE_CUBE_FACE_Z_NEG:	camera.Tilt( 0.5f * PI );	break;
			}

			mat4x4f& viewMatrix = m_viewMatrices[ faceId ];

			viewMatrix = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order
			viewMatrix[ 3 ][ 3 ] = 0.0f;

			m_mipProcesses[ faceId ] = mipProcess;

			m_mipProcesses[ faceId ]->SetConstants( &viewMatrix, sizeof( mat4x4f ) );
		}
	}
}


void MipCubeProcess::Resize()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId )
	{
		m_outputFaceImage[ faceId ].Resize();
		m_mipProcesses[ faceId ]->Resize();
	}
}


void MipCubeProcess::Shutdown()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId )
	{
		m_outputFaceImage[ faceId ].Destroy();
		m_mipProcesses[ faceId ]->Shutdown();
		delete m_mipProcesses[ faceId ];
	}
}


void MipCubeProcess::FrameBegin()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_mipProcesses[ faceId ]->FrameBegin();
	}
}


void MipCubeProcess::FrameEnd()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_mipProcesses[ faceId ]->FrameEnd();
	}
}


void MipCubeProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( Color::Purple ) );

	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_mipProcesses[ faceId ]->Execute( cmdContext );
	}

	cmdContext.MarkerEndRegion();
}