#include "postEffect.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"

extern AssetManager g_assets;

void ImageProcess::Init( const imageProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "ImageProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	m_dbgName = info.name;

	pass = new PostPass( info.fb );

	pass->codeImages.Resize( 3 );

	m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

	m_transitionState = {};
	m_transitionState.flags.clear = info.clear;
	m_transitionState.flags.store = true;
	m_transitionState.flags.presentAfter = info.present;
	m_transitionState.flags.readAfter = !info.present;
	m_transitionState.flags.readOnly = true;

	m_resources = info.resources;
	m_context = info.context;

	assert( info.progHdl != INVALID_HDL );
	m_progAsset = g_assets.gpuPrograms.Find( info.progHdl );

	buffer.Create( "Resource buffer", LIFETIME_TEMP, 1, sizeof( imageProcessObject_t ), bufferType_t::UNIFORM, m_context->sharedMemory );
}


void ImageProcess::SetSourceImage( const uint32_t slot, Image* image )
{
	pass->codeImages[ slot ] = image;
}


void ImageProcess::Resize()
{
	pass->SetViewport( 0, 0, pass->GetFrameBuffer()->GetWidth(), pass->GetFrameBuffer()->GetHeight() );
}


void ImageProcess::Shutdown()
{
	buffer.Destroy();

	delete pass;
}


void ImageProcess::FrameBegin()
{
	{
		const float w = float( pass->GetFrameBuffer()->GetWidth() );
		const float h = float( pass->GetFrameBuffer()->GetHeight() );

		imageProcessObject_t process = {};
		process.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );

		buffer.SetPos( 0 );
		buffer.CopyData( &process, sizeof( imageProcessObject_t ) );
	}

	pass->parms->Bind( bind_sourceImages, &pass->codeImages );
	pass->parms->Bind( bind_imageStencil, &m_resources->stencilImageView ); // FIXME: allow either special desc sets or null inputs
	pass->parms->Bind( bind_imageProcess, &buffer );
}


void ImageProcess::FrameEnd()
{

}


void ImageProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::White ) );

	hdl_t pipeLineHandle = CreateGraphicsPipeline( cmdContext.GetRenderContext(), pass, *m_progAsset );

	vk_RenderImageShader( cmdContext, pipeLineHandle, pass );

	cmdContext.MarkerEndRegion();
}