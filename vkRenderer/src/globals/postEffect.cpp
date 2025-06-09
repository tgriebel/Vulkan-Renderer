#include "postEffect.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"

extern AssetManager g_assets;

void ImageProcess::Init( const imageProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "ImageProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	m_dbgName = info.name;

	m_pass = new PostPass( info.fb );

	m_pass->codeImages.Resize( info.inputImages );
	m_pass->codeCubeImages.Resize( info.inputCubeImages );

	m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

	m_transitionState = {};
	m_transitionState.flags.clear = info.clear;
	m_transitionState.flags.store = true;
	m_transitionState.flags.presentAfter = info.present;
	m_transitionState.flags.readAfter = !info.present;
	m_transitionState.flags.readOnly = true;

	m_resources = info.resources;
	m_context = info.context;

	m_callback = info.callback;

	assert( info.progHdl != INVALID_HDL );
	m_progAsset = g_assets.gpuPrograms.Find( info.progHdl );

	m_buffer.Create( "Resource buffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::UNMANAGED, 1, MaxBufferSizeInBytes, bufferType_t::UNIFORM, m_context->sharedMemory );

	m_pass->parms = m_context->RegisterBindParm( bindset_imageProcess );
}


void ImageProcess::SetSourceImage( const uint32_t slot, Image* image )
{
	assert( image->info.type == imageType_t::IMAGE_TYPE_2D );
	assert( m_pass->codeImages.Count() > slot );
	if( slot < m_pass->codeImages.Count() ) {
		m_pass->codeImages[ slot ] = image;
	}
}


void ImageProcess::SetSourceCubeImage( const uint32_t slot, Image* image )
{
	assert( image->info.type == imageType_t::IMAGE_TYPE_CUBE );
	assert( m_pass->codeCubeImages.Count() > slot );
	if ( slot < m_pass->codeCubeImages.Count() ) {
		m_pass->codeCubeImages[ slot ] = image;
	}
}


void ImageProcess::SetConstants( const void* dataBlock, const uint32_t sizeInBytes )
{
	assert( sizeInBytes <= MaxConstantBlockSizeInBytes );
	m_buffer.SetPos( ReservedConstantSizeInBytes );
	m_buffer.CopyData( dataBlock, Min( sizeInBytes, MaxConstantBlockSizeInBytes ) );
}


void ImageProcess::Resize()
{
	m_pass->SetViewport( 0, 0, m_pass->GetFrameBuffer()->GetWidth(), m_pass->GetFrameBuffer()->GetHeight() );
}


void ImageProcess::Shutdown()
{
	m_buffer.Destroy();

	delete m_pass;
}


void ImageProcess::FrameBegin()
{
	// Set standard constants
	{
		const viewport_t& viewport = m_pass->GetViewport();
		const float w = float( viewport.width );
		const float h = float( viewport.height );

		vec4f dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );

		const uint64_t offset = m_buffer.GetSize();
		m_buffer.SetPos( 0 );
		m_buffer.CopyData( &dimensions, sizeof( vec4f ) );
		m_buffer.SetPos( offset );
	}

	// Set standard binds
	{
		m_pass->parms->Bind( bind_sourceImages,		m_pass->codeImages.Count() > 0 ? &m_pass->codeImages : &rc.defaultImageArray );
		m_pass->parms->Bind( bind_sourceCubeImages, m_pass->codeCubeImages.Count() > 0 ? m_pass->codeCubeImages[ 0 ] : rc.defaultImageCube );
		m_pass->parms->Bind( bind_imageStencil,		&m_resources->stencilImageView ); // FIXME: allow either special desc sets or null inputs
		m_pass->parms->Bind( bind_imageProcess,		&m_buffer );
	}

	// Allow custom constants/binds
	if ( m_callback != nullptr ) {
		( *m_callback )( this );
	}
}


void ImageProcess::FrameEnd()
{

}


void ImageProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::White ) );

	m_pass->InsertResourceBarriers( cmdContext );

	hdl_t pipeLineHandle = CreateGraphicsPipeline( cmdContext.GetRenderContext(), m_pass, *m_progAsset );

	vk_RenderImageShader( cmdContext, pipeLineHandle, m_pass, m_transitionState );

	cmdContext.MarkerEndRegion();
}