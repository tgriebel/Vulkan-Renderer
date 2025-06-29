#include "../render_core/renderer.h"
#include "renderview.h"
#include "../render_binding/bindings.h"

#if defined( USE_IMGUI )
#include "../../external/imgui/imgui.h"
#endif

#include "../../window.h"
#include "../../input.h"
#include "../render_core/debugMenu.h"

extern Scene*	g_scene;
extern Window	g_window;

static void DrawDebugMenu( RenderView& view )
{
#if defined( USE_IMGUI )
	ImGui::Begin( "Control Panel" );

	if ( ImGui::BeginTabBar( "Tabs" ) )
	{
		for ( uint32_t i = 0; i < view.debugMenus.Count(); ++i ) {
			( *view.debugMenus[ i ] )( );
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
#endif
}


void RenderView::Init( const renderViewCreateInfo_t& info )
{
	const uint32_t frameStateCount = MaxFrameStates;

	m_name = info.name;
	m_region = info.region;
	m_resources = info.resources;
	m_surfaceBufferId = info.viewId;
	m_viewBufferId = info.viewId;

	m_multiViewCount = info.multiViewCount;

	for ( uint32_t multiViewIndex = 0; multiViewIndex < info.multiViewCount; ++multiViewIndex )
	{
		frameBufferCreateInfo_t fbInfo;
		fbInfo.name = m_name;
		fbInfo.color0 = info.color[ multiViewIndex ];
		fbInfo.color1 = info.gBuffer0[ multiViewIndex ];
		fbInfo.color2 = info.gBuffer1[ multiViewIndex ];
		fbInfo.depth = info.depth[ multiViewIndex ];
		fbInfo.stencil = info.stencil[ multiViewIndex ];
		fbInfo.swapBuffering = info.swapBuffering;

		m_framebuffers[ multiViewIndex ] = new FrameBuffer();
		m_framebuffers[ multiViewIndex ]->Create( fbInfo );
	}

	m_viewParms = info.context->RegisterBindParm( bindset_view );

	for ( uint32_t multiViewIndex = 0; multiViewIndex < info.multiViewCount; ++multiViewIndex )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
			passes[ multiViewIndex ][ passIx ] = nullptr;
		}
	}

	const uint32_t beginPass = ViewRegionPassBegin();
	const uint32_t endPass = ViewRegionPassEnd();

	for ( uint32_t multiViewIndex = 0; multiViewIndex < info.multiViewCount; ++multiViewIndex )
	{
		FrameBuffer* fb = m_framebuffers[ multiViewIndex ];

		for ( uint32_t passIx = beginPass; passIx <= endPass; ++passIx )
		{
			switch( passIx )
			{
				case DRAWPASS_SHADOW:
					passes[ multiViewIndex ][ passIx ] = new ShadowPass( fb );
					break;
				case DRAWPASS_DEPTH:
					passes[ multiViewIndex ][ passIx ] = new DepthPass( fb );
					break;
				case DRAWPASS_TERRAIN:
					passes[ multiViewIndex ][ passIx ] = new TerrainPass( fb );
					break;
				case DRAWPASS_OPAQUE:
					passes[ multiViewIndex ][ passIx ] = new OpaquePass( fb );
					break;
				case DRAWPASS_SKYBOX:
					passes[ multiViewIndex ][ passIx ] = new SkyboxPass( fb );
					break;
				case DRAWPASS_TRANS:
					passes[ multiViewIndex ][ passIx ] = new TransPass( fb );
					break;
				case DRAWPASS_EMISSIVE:
					passes[ multiViewIndex ][ passIx ] = new EmissivePass( fb );
					break;
				case DRAWPASS_DEBUG_3D:
					passes[ multiViewIndex ][ passIx ] = new Debug3dPass( fb );
					break;
				case DRAWPASS_DEBUG_WIREFRAME:
					passes[ multiViewIndex ][ passIx ] = new WireframePass( fb );
					break;
				case DRAWPASS_2D:
					passes[ multiViewIndex ][ passIx ] = new PostPass( fb );
					break;
				case DRAWPASS_DEBUG_2D:
					passes[ multiViewIndex ][ passIx ] = new Debug2dPass( fb );
					break;
			}

			const ShaderBindSet* bindset_pass = info.context->LookupBindSet( "bindset_pass"  );
			passes[ multiViewIndex ][ passIx ]->parms = info.context->RegisterBindParm( bindset_pass );
		}
	}


	if( info.region == renderViewRegion_t::SHADOW )
	{
		m_transitionState.flags.clear = true;
		m_transitionState.flags.store = true;
		m_transitionState.flags.readOnly = true;
		m_transitionState.flags.readAfter = true;
		m_transitionState.flags.presentBefore = false;
		m_transitionState.flags.presentAfter = false;

		m_clearColor = vec4f( 0.0f, 0.0f, 0.0f, 1.0f );
		m_clearDepth = 1.0f;
		m_clearStencil = 0;
	}
	else if( info.region == renderViewRegion_t::STANDARD_RASTER )
	{
		m_transitionState.flags.clear = true;
		m_transitionState.flags.store = true;
		m_transitionState.flags.readOnly = true;
		m_transitionState.flags.readAfter = true;
		m_transitionState.flags.presentBefore = false;
		m_transitionState.flags.presentAfter = false;

		m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
		m_clearDepth = 0.0f;
		m_clearStencil = 0;
	}
	else if ( info.region == renderViewRegion_t::STANDARD_2D )
	{
		m_transitionState.flags.clear = true;
		m_transitionState.flags.store = true;
		m_transitionState.flags.readOnly = false;
		m_transitionState.flags.readAfter = false;
		m_transitionState.flags.presentBefore = true;
		m_transitionState.flags.presentAfter = true;

		m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );
		m_clearDepth = 0.0f;
		m_clearStencil = 0;
	}


}


void RenderView::FrameBegin()
{
	m_viewParms->Bind( bind_modelBuffer, &m_resources->surfParmPartitions[ m_surfaceBufferId ] );

	for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = passes[ multiViewIndex ][ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			pass->FrameBegin( m_resources );
		}
	}

	DrawDebugMenu( *this );
}


void RenderView::FrameEnd()
{
	for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = passes[ multiViewIndex ][ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			pass->FrameEnd();
		}
	}
}


void RenderView::Resize()
{
	for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = passes[ multiViewIndex ][ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			pass->SetViewport( 0, 0, pass->GetFrameBuffer()->GetWidth(), pass->GetFrameBuffer()->GetHeight() );
		}
	}
}


void RenderView::SetViewRect( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height )
{
	m_viewport.x = x;
	m_viewport.y = y;
	m_viewport.width = width;
	m_viewport.height = height;
}


drawPass_t RenderView::ViewRegionPassBegin() const
{
	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_BEGIN;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_BEGIN;
		case renderViewRegion_t::STANDARD_2D:		return DRAWPASS_2D_BEGIN;
	}
	return DRAWPASS_COUNT;
}


drawPass_t RenderView::ViewRegionPassEnd() const
{
	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:			return DRAWPASS_SHADOW_END;
		case renderViewRegion_t::STANDARD_RASTER:	return DRAWPASS_MAIN_END;
		case renderViewRegion_t::STANDARD_2D:		return DRAWPASS_2D_END;
	}
	return DRAWPASS_COUNT;
}


renderPassTransition_t RenderView::TransitionState() const
{
	return m_transitionState;
}


const vec4f& RenderView::ClearColor() const
{
	return m_clearColor;
}


float RenderView::ClearDepth() const
{
	return m_clearDepth;
}


uint32_t RenderView::ClearStencil() const
{
	return m_clearStencil;
}


const ShaderBindParms* RenderView::BindParms() const
{
	return m_viewParms;
}


vec2i RenderView::GetFrameSize() const
{
	if( m_framebuffers[ 0 ] == nullptr ) {
		return vec2i( 0, 0 );
	}
	return vec2i( static_cast<int32_t>( m_framebuffers[ 0 ]->GetWidth() ), static_cast<int32_t>( m_framebuffers[ 0 ]->GetHeight() ) );
}


const viewport_t& RenderView::GetViewport() const
{
	return m_viewport;
}


const mat4x4f& RenderView::GetViewMatrix( const uint32_t multiView ) const
{
	return m_viewMatrices[ multiView ];
}


const mat4x4f& RenderView::GetProjMatrix( const uint32_t multiView ) const
{
	return m_projMatrices[ multiView ];
}


const mat4x4f& RenderView::GetViewprojMatrix( const uint32_t multiView ) const
{
	return m_viewprojMatrices[ multiView ];
}


int RenderView::GetViewBufferId( const int multiView ) const
{
	return m_viewBufferId + multiView;
}


int RenderView::GetSurfaceBufferId() const
{
	return m_surfaceBufferId;
}


uint32_t RenderView::GetMultiViewCount() const
{
	return m_multiViewCount;
}


const char* RenderView::GetName() const
{
	return m_name;
}


const renderViewRegion_t RenderView::GetRegion() const
{
	return m_region;
}


const void RenderView::Commit()
{
	m_committed = true;;
}


bool RenderView::IsCommitted() const
{
	return m_committed;
}


void RenderView::SetCamera( const Camera& camera, const bool reverseZ, const uint32_t multiView )
{
	m_viewMatrices[ multiView ] = camera.GetViewMatrix();
	m_projMatrices[ multiView ] = camera.GetPerspectiveMatrix( reverseZ );
	m_viewprojMatrices[ multiView ] = m_projMatrices[ multiView ] * m_viewMatrices[ multiView ];

	m_viewport.near = camera.GetNearClip();
	m_viewport.far = camera.GetFarClip();
}

void RenderView::AttachDebugMenu( const debugMenuFuncPtr funcPtr )
{
	debugMenus.Append( funcPtr );
}