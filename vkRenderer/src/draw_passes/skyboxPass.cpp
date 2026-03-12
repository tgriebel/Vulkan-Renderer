#include "drawpass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void SkyboxPass::Init( FrameBuffer* frameBuffer )
{
	m_name = "Skybox Pass";
	m_passId = DRAWPASS_SKYBOX;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_DEPTH_WRITE;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;

	SetFrameBuffer( frameBuffer );
}


void SkyboxPass::FrameBegin( const ResourceContext* resources )
{
	parms->Bind( bind_lightBuffer, &resources->lightParms );
	parms->Bind( bind_imageCodeArray, &codeImages );
	parms->Bind( bind_imageCodeCubeArray, &codeCubeImages );
	parms->Bind( bind_imageStencil, rc.whiteImage );
}


void SkyboxPass::FrameEnd()
{

}