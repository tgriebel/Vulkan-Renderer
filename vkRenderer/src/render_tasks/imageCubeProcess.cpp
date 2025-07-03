#include "imageCubeProcess.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"

extern AssetManager g_assets;

std::string ImageCubeProcess::AsString() const
{
	std::stringstream ss;
	ss << "<ImageCubeProcess: " << m_dbgName << ">";
	return ss.str();
}


void ImageCubeProcess::Init( const imageCubeProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "ImageCubeProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	assert( ( info.image->info.layers == 6 ) && ( info.image->info.type == IMAGE_TYPE_CUBE ) );

	{
		imageProcessCreateInfo_t subInfo{};
		subInfo.name = info.name;
		subInfo.image = info.image;
		subInfo.progHdl = info.progHdl;
		subInfo.context = info.context;
		subInfo.resources = info.resources;
		subInfo.inputCubeImages = info.inputCubeImages;
		subInfo.resolve = false;
		subInfo.present = false;
		subInfo.clear = info.clear;

		GpuTask* child = this;
		for( uint32_t i = 0; i < 6; ++i )
		{
			ImageProcess* imageProcess = new ImageProcess( subInfo );

			Camera camera = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
			camera.SetFov( Radians( 90.0f ) );
			camera.SetAspectRatio( 1.0f );

			switch ( i )
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

			imageProcess->SetConstants( &viewMatrix, sizeof( mat4x4f ) );

			child->SetChild( imageProcess );
			child = imageProcess;
		}
	}
}


void ImageCubeProcess::SetSourceImage( const uint32_t slot, Image* image )
{
	for ( uint32_t i = 0; i < 6; ++i )
	{
		ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
		child->SetSourceImage( slot, image );
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::SetSourceCubeImage( const uint32_t slot, Image* image )
{
	for ( uint32_t i = 0; i < 6; ++i )
	{
		ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
		child->SetSourceCubeImage( slot, image );
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::Resize()
{
	for ( uint32_t i = 0; i < 6; ++i )
	{
		GpuTask* child = m_child;
		child->Resize();
		child = child->GetChild();
	}
}


void ImageCubeProcess::Shutdown()
{
	for ( uint32_t i = 0; i < 6; ++i )
	{
		ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
		child->Shutdown();
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::FrameBegin()
{
	for ( uint32_t i = 0; i < 6; ++i )
	{
		ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
		child->FrameBegin();
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::FrameEnd()
{
	for ( uint32_t i = 0; i < 6; ++i )
	{
		ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
		child->FrameEnd();
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::White ) );

	for ( uint32_t i = 0; i < 6; ++i )
	{
		ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
		child->Execute( cmdContext );
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}

	cmdContext.MarkerEndRegion();
}