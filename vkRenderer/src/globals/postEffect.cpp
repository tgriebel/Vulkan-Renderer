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

	m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

	m_transitionState = {};
	m_transitionState.flags.clear = info.clear;
	m_transitionState.flags.store = true;
	m_transitionState.flags.presentAfter = info.present;
	m_transitionState.flags.readAfter = !info.present;
	m_transitionState.flags.readOnly = true;

	m_constants[ 0 ] = info.constants[ 0 ];
	m_constants[ 1 ] = info.constants[ 1 ];
	m_constants[ 2 ] = info.constants[ 2 ];

	m_resources = info.resources;
	m_context = info.context;

	assert( info.progHdl != INVALID_HDL );
	m_progAsset = g_assets.gpuPrograms.Find( info.progHdl );

	m_buffer.Create( "Resource buffer", LIFETIME_TEMP, 1, sizeof( imageProcessObject_t ), bufferType_t::UNIFORM, m_context->sharedMemory );

	m_pass->parms = m_context->RegisterBindParm( bindset_imageProcess );
}


void ImageProcess::SetSourceImage( const uint32_t slot, Image* image )
{
	assert( m_pass->codeImages.Count() > slot );
	if( slot < m_pass->codeImages.Count() ) {
		m_pass->codeImages[ slot ] = image;
	}
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
	{
		const viewport_t& viewport = m_pass->GetViewport();
		const float w = float( viewport.width );
		const float h = float( viewport.height );

		imageProcessObject_t process = {};
		process.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );
		process.generic0 = m_constants[ 0 ];
		process.generic1 = m_constants[ 1 ];
		process.generic2 = m_constants[ 2 ];

		m_buffer.SetPos( 0 );
		m_buffer.CopyData( &process, sizeof( imageProcessObject_t ) );
	}

	m_pass->parms->Bind( bind_sourceImages, &m_pass->codeImages );
	m_pass->parms->Bind( bind_imageStencil, &m_resources->stencilImageView ); // FIXME: allow either special desc sets or null inputs
	m_pass->parms->Bind( bind_imageProcess, &m_buffer );
}


void ImageProcess::FrameEnd()
{

}


void ImageProcess::Execute( CommandContext& cmdContext )
{
	const uint32_t codeImageCount = m_pass->codeImages.Count();
	for( uint32_t i = 0; i < codeImageCount; ++i ) {
		// HACK: depth-stencil transitions are creating validation issues
		if( ( m_pass->codeImages[ i ]->info.aspect & IMAGE_ASPECT_DEPTH_FLAG ) != 0 ) {
			continue;
		}
		if ( ( m_pass->codeImages[ i ]->info.aspect & IMAGE_ASPECT_STENCIL_FLAG ) != 0 ) {
			continue;
		}
		Transition( &cmdContext, *const_cast<Image*>(m_pass->codeImages[ i ]), GPU_IMAGE_WRITE, GPU_IMAGE_READ );
	}
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::White ) );

	hdl_t pipeLineHandle = CreateGraphicsPipeline( cmdContext.GetRenderContext(), m_pass, *m_progAsset );

	vk_RenderImageShader( cmdContext, pipeLineHandle, m_pass, m_transitionState );

	cmdContext.MarkerEndRegion();
}