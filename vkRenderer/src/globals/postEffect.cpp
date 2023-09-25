#include "postEffect.h"

#include <gfxcore/scene/scene.h>
#include "../render_core/renderer.h"

extern AssetManager g_assets;

void ImageProcess::Init( const imageProcessCreateInfo_t& info )
{
	ScopedLogTimer timer( "ImageProcessInit", timerPrecision_t::MICROSECOND, &TimerPrint );

	m_dbgName = info.name;

	pass = new PostPass();

	pass->name = "Fullscreen Pass";
	pass->viewport.x = 0;
	pass->viewport.y = 0;
	pass->viewport.width = info.fb->GetWidth();
	pass->viewport.height = info.fb->GetHeight();
	pass->fb = info.fb;

	m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

	pass->stateBits |= GFX_STATE_BLEND_ENABLE;
	pass->stateBits |= GFX_STATE_MSAA_ENABLE;
	pass->sampleRate = info.fb->GetColor()->info.subsamples;

	m_transitionState = {};
	m_transitionState.flags.clear = info.clear;
	m_transitionState.flags.store = true;
	m_transitionState.flags.presentAfter = info.present;
	m_transitionState.flags.readAfter = !info.present;
	m_transitionState.flags.readOnly = true;

	assert( info.progHdl != INVALID_HDL );
	m_progAsset = g_assets.gpuPrograms.Find( info.progHdl );

	buffer.Create( "Resource buffer", LIFETIME_TEMP, 1, sizeof( imageProcessObject_t ), bufferType_t::UNIFORM, info.context->sharedMemory );
}


void ImageProcess::Shutdown()
{
	buffer.Destroy();

	delete pass;
}


void ImageProcess::Execute( CommandContext& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::White ) );

	{
		const float w = float( pass->fb->GetWidth() );
		const float h = float( pass->fb->GetHeight() );

		imageProcessObject_t process = {};
		process.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );

		buffer.SetPos( 0 );
		buffer.CopyData( &process, sizeof( imageProcessObject_t ) );
	}

	vk_RenderImageShader( cmdContext, m_progAsset, pass );

	cmdContext.MarkerEndRegion();
}