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
		subInfo.image = info.image;
		subInfo.progHdl = info.progHdl;
		subInfo.context = info.context;
		subInfo.resources = info.resources;
		subInfo.inputCubeImages = info.inputCubeImages;
		subInfo.resolve = false;
		subInfo.present = false;
		subInfo.clear = info.clear;
		subInfo.mipLevel = 0;

		GpuTask* child = this;
		for( uint32_t i = 0; i < 6; ++i )
		{
			subInfo.layer = vk_MapToGlslCubemapConvention( i );

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
	ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
	for ( uint32_t i = 0; i < 6; ++i )
	{
		child->SetSourceImage( slot, image );
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::SetSourceCubeImage( const uint32_t slot, Image* image )
{
	ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );
	for ( uint32_t i = 0; i < 6; ++i )
	{
		child->SetSourceCubeImage( slot, image );
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::Resize()
{
	GpuTask* child = m_child;
	for ( uint32_t i = 0; i < 6; ++i )
	{		
		child->Resize();
		child = child->GetChild();
	}
}


void ImageCubeProcess::Shutdown()
{
	ImageProcess* child = reinterpret_cast<ImageProcess*>( m_child );

	for ( uint32_t i = 0; i < 6; ++i )
	{
		child->Shutdown();
		child = reinterpret_cast<ImageProcess*>( child->GetChild() );
	}
}


void ImageCubeProcess::FrameBegin()
{
	GpuTask* child = m_child;

	for ( uint32_t i = 0; i < 6; ++i )
	{		
		child->FrameBegin();
		child = child->GetChild();
	}
}


void ImageCubeProcess::FrameEnd()
{
	GpuTask* child = m_child;

	for ( uint32_t i = 0; i < 6; ++i )
	{
		child->FrameEnd();
		child = child->GetChild();
	}
}


void ImageCubeProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( Color::White ) );

	GpuTask* child = m_child;

	for ( uint32_t i = 0; i < 6; ++i )
	{
		child->Execute( cmdContext );
		child = child->GetChild();
	}

	cmdContext.MarkerEndRegion();
}