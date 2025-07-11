#include "imageCubeProcess.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"

extern AssetManager g_assets;

std::string ImageCubeProcess::AsString() const
{
	std::stringstream ss;
	ss << "<ImageCubeProcess: " << m_name << ">";
	return ss.str();
}


void ImageCubeProcess::Init( const imageCubeProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "ImageCubeProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	assert( ( info.image->info.layers == 6 ) && ( info.image->info.type == IMAGE_TYPE_CUBE ) );

	m_name = info.name;
	{
		imageProcessCreateInfo_t subInfo{};
		subInfo.name = info.name;
		subInfo.outputImage = info.image;
		subInfo.progHdl = info.progHdl;
		subInfo.context = info.context;
		subInfo.resources = info.resources;
		subInfo.resolve = false;
		subInfo.present = false;
		subInfo.clear = info.clear;
		subInfo.mipLevel = 0;
		subInfo.inputCubeImages = 1;

		for( uint32_t faceId = 0; faceId < 6; ++faceId )
		{
			subInfo.layer = vk_MapToGlslCubemapConvention( faceId );

			ImageProcess* imageProcess = new ImageProcess( subInfo );

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

			mat4x4f viewMatrix = camera.GetViewMatrix().Transpose(); // FIXME: row/column-order
			viewMatrix[ 3 ][ 3 ] = 0.0f;

			//for ( uint32_t j = 0; j < 4; ++j ) {
			//	for ( uint32_t i = 0; i < 4; ++i ) {
			//		viewMatrix[ j ][ i ] = i + j * 4;
			//	}
			//}

			imageProcess->SetConstants( &viewMatrix, sizeof( mat4x4f ) );

			m_imgProcesses[ faceId ] = imageProcess;
		}
	}
}


void ImageCubeProcess::SetSourceImage( const uint32_t slot, Image* image )
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_imgProcesses[ faceId ]->SetSourceImage( slot, image );
	}
}


void ImageCubeProcess::SetSourceCubeImage( const uint32_t slot, Image* image )
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_imgProcesses[ faceId ]->SetSourceCubeImage( slot, image );
	}
}


void ImageCubeProcess::Resize()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {		
		m_imgProcesses[ faceId ]->Resize();
	}
}


void ImageCubeProcess::Shutdown()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId )
	{
		m_imgProcesses[ faceId ]->Shutdown();
		delete m_imgProcesses[ faceId ];
	}
}


void ImageCubeProcess::FrameBegin()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {		
		m_imgProcesses[ faceId ]->FrameBegin();
	}
}


void ImageCubeProcess::FrameEnd()
{
	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_imgProcesses[ faceId ]->FrameEnd();
	}
}


void ImageCubeProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( Color::White ) );

	for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
		m_imgProcesses[ faceId ]->Execute( cmdContext );
	}

	cmdContext.MarkerEndRegion();
}