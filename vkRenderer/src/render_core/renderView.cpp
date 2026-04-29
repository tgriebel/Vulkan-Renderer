#include "../render_core/renderer.h"
#include "renderview.h"
#include "../render_binding/bindings.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#endif

#include "../app/window.h"
#include "../app/input.h"
#include "../render_core/debugMenu.h"

#include "../draw_passes/debug2dPass.h"
#include "../draw_passes/debug3dPass.h"
#include "../draw_passes/depthPass.h"
#include "../draw_passes/opaquePass.h"
#include "../draw_passes/transPass.h"
#include "../draw_passes/shadowPass.h"
#include "../draw_passes/skyboxPass.h"
#include "../draw_passes/terrainPass.h"
#include "../draw_passes/postPass.h"
#include "../draw_passes/wireFramePass.h"

extern Scene*	g_scene;
extern Window	g_window;

void DrawDebugMenu( RenderView& view )
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
	m_fbSourceImages = info.fbImages;
	m_region = info.region;
	m_resources = info.resources;
	m_surfaceBufferId = info.viewId;
	m_viewBufferId = info.viewId;

	m_multiViewCount = info.isCubeView ? 6 : 1;
	m_isCubeView = info.isCubeView;

	m_context = info.context;
	m_viewParms = m_context->RegisterBindParm( bindset_view );

	for ( uint32_t multiViewIndex = 0; multiViewIndex < m_multiViewCount; ++multiViewIndex )
	{
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx ) {
			passes[ multiViewIndex ][ passIx ] = nullptr;
		}
		m_framebuffers[ multiViewIndex ] = new FrameBuffer();

		if ( info.fbImages.color0 != nullptr ) {
			m_colorViews[ multiViewIndex ] = new ImageView();
		}
		if ( info.fbImages.color1 != nullptr ) {
			m_gBuffer0Views[ multiViewIndex ] = new ImageView();
		}
		if ( info.fbImages.color2 != nullptr ) {
			m_gBuffer1Views[ multiViewIndex ] = new ImageView();
		}
		if ( info.fbImages.depth != nullptr ) {
			m_depthViews[ multiViewIndex ] = new ImageView();
		}
		if ( info.fbImages.stencil != nullptr ) {
			m_stencilViews[ multiViewIndex ] = new ImageView();
		}
	}

	CreateFrameBuffers( info.fbImages );

	uint32_t beginPass;
	uint32_t endPass;

	switch ( m_region )
	{
		case renderViewRegion_t::SHADOW:
		{
			beginPass = DRAWPASS_SHADOW_BEGIN;
			endPass = DRAWPASS_SHADOW_END;
		} break;
		case renderViewRegion_t::STANDARD_RASTER:
		{
			beginPass = DRAWPASS_MAIN_BEGIN;
			endPass = DRAWPASS_MAIN_END;
		} break;
		case renderViewRegion_t::STANDARD_2D:
		{
			beginPass = DRAWPASS_2D_BEGIN;
			endPass = DRAWPASS_2D_END;
		} break;
	}

	for ( uint32_t multiViewIndex = 0; multiViewIndex < m_multiViewCount; ++multiViewIndex )
	{
		FrameBuffer* fb = m_framebuffers[ multiViewIndex ];

		for ( uint32_t passIx = beginPass; passIx <= endPass; ++passIx )
		{
			switch( passIx )
			{
				case DRAWPASS_SHADOW:
					passes[ multiViewIndex ][ passIx ] = new ShadowPass( m_context, fb );
					break;
				case DRAWPASS_DEPTH:
					passes[ multiViewIndex ][ passIx ] = new DepthPass( m_context, fb );
					break;
				case DRAWPASS_TERRAIN:
					passes[ multiViewIndex ][ passIx ] = new TerrainPass( m_context, fb );
					break;
				case DRAWPASS_OPAQUE:
					passes[ multiViewIndex ][ passIx ] = new OpaquePass( m_context, fb );
					break;
				case DRAWPASS_SKYBOX:
					passes[ multiViewIndex ][ passIx ] = new SkyboxPass( m_context, fb );
					break;
				case DRAWPASS_TRANS:
					passes[ multiViewIndex ][ passIx ] = new TransPass( m_context, fb );
					break;
				case DRAWPASS_DEBUG_3D:
					passes[ multiViewIndex ][ passIx ] = new Debug3dPass( m_context, fb );
					break;
				case DRAWPASS_DEBUG_WIREFRAME:
					passes[ multiViewIndex ][ passIx ] = new WireframePass( m_context, fb );
					break;
				case DRAWPASS_2D:
					passes[ multiViewIndex ][ passIx ] = new PostPass( m_context, fb );
					break;
				case DRAWPASS_DEBUG_2D:
					passes[ multiViewIndex ][ passIx ] = new Debug2dPass( m_context, fb );
					break;
			}

			const ShaderBindSet* bindset_pass = m_context->LookupBindSet( "bindset_pass"  );
			passes[ multiViewIndex ][ passIx ]->parms = m_context->RegisterBindParm( bindset_pass );
		}
	}

	m_transitionState = info.transition;
	
	m_clearImage = info.clear;
	m_finalizeImage = info.finalize;

	if( m_clearImage )
	{
		m_clearColor = info.clearColor;
		m_clearDepth = info.clearDepth;
		m_clearStencil = info.clearStencil;
	}

	m_viewParmeters = m_resources->viewParms.GetView( m_viewBufferId, 1 );
	m_surfParmeters = m_resources->surfParms.GetView( m_surfaceBufferId * MaxSurfaces, MaxSurfaces );
}


void RenderView::CreateFrameBuffers( const frameBufferCreateInfo_t& info )
{
	m_fbSourceImages = info;

	for ( uint32_t multiViewIndex = 0; multiViewIndex < m_multiViewCount; ++multiViewIndex )
	{
		imageSubResourceView_t subView;
		subView.arrayCount = 1;
		subView.baseArray = m_isCubeView ? vk_MapToGlslCubemapConvention( multiViewIndex ) : 0; // This is all that changes for cube views
		subView.baseMip = 0;
		subView.mipLevels = 1;

		if( info.color0 != nullptr )
		{
			imageInfo_t colorInfo = info.color0->info;
			colorInfo.type = IMAGE_TYPE_2D;

			m_colorViews[ multiViewIndex ]->Init( info.color0, colorInfo, subView, resourceLifeTime_t::RESIZE );
		}

		if ( info.color1 != nullptr )
		{
			imageInfo_t gbufferInfo = info.color1->info;
			gbufferInfo.type = IMAGE_TYPE_2D;

			m_gBuffer0Views[ multiViewIndex ]->Init( info.color1, gbufferInfo, subView, resourceLifeTime_t::RESIZE );
		}

		if ( info.color2 != nullptr )
		{
			imageInfo_t gbufferInfo = info.color2->info;
			gbufferInfo.type = IMAGE_TYPE_2D;

			m_gBuffer1Views[ multiViewIndex ]->Init( info.color2, gbufferInfo, subView, resourceLifeTime_t::RESIZE );
		}

		if ( info.depth != nullptr )
		{
			imageInfo_t depthInfo = info.depth->info;
			depthInfo.type = IMAGE_TYPE_2D;

			m_depthViews[ multiViewIndex ]->Init( info.depth, depthInfo, subView, resourceLifeTime_t::RESIZE );
		}

		if ( info.stencil != nullptr )
		{
			imageInfo_t stencilInfo = info.stencil->info;
			stencilInfo.type = IMAGE_TYPE_2D;

			m_stencilViews[ multiViewIndex ]->Init( info.stencil, stencilInfo, subView, resourceLifeTime_t::RESIZE );
		}

		frameBufferCreateInfo_t fbInfo {};
		fbInfo.name = info.name;
		fbInfo.swapBuffering = info.swapBuffering;
		fbInfo.context = m_context;
		fbInfo.color0 = m_colorViews[ multiViewIndex ];
		fbInfo.color1 = m_gBuffer0Views[ multiViewIndex ];
		fbInfo.color2 = m_gBuffer1Views[ multiViewIndex ];
		fbInfo.depth = m_depthViews[ multiViewIndex ];
		fbInfo.stencil = m_stencilViews[ multiViewIndex ];
		
		m_framebuffers[ multiViewIndex ]->Create( fbInfo );
	}
}


void RenderView::FrameBegin( const drawPass_t begin, const drawPass_t end )
{
	if( IsCommitted() == false )
	{
		return;
	}

	const uint64_t currentFrame = m_context->FrameNumber();

	// View data
	{	
		const uint32_t multiViewCount = GetMultiViewCount();
		for( uint32_t multiViewIndex = 0; multiViewIndex < multiViewCount; ++multiViewIndex )
		{
			gpuView_t viewBuffer = {};

			const vec2i& frameSize = GetFrameSize();
			viewBuffer.viewMat = GetViewMatrix( multiViewIndex );
			viewBuffer.projMat = GetProjMatrix( multiViewIndex );
			viewBuffer.prevViewProjMat = GetPreviousViewProjMatrix( multiViewIndex );
			viewBuffer.viewProjMat = GetViewProjMatrix( multiViewIndex );
			viewBuffer.viewOrigin = GetViewOrigin();
			viewBuffer.dimensions = vec4f( (float)frameSize[ 0 ], (float)frameSize[ 1 ], 1.0f / frameSize[ 0 ], 1.0f / frameSize[ 1 ] );
			viewBuffer.numLights = numLights;

			m_viewParmeters.SetPos( 0 );
			m_viewParmeters.CopyData( &viewBuffer, sizeof( viewBuffer ) );
		}
	}

	// Per-surface data
	// TODO: Push into DrawPass since this is draw group specific
	{
		const uint32_t viewId = GetSurfaceBufferId();

		for( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			assert( drawGroup[ passIx ].InstanceCount() < MaxSurfaces );

			if( drawGroup[ passIx ].InstanceCount() == 0 )
			{
				continue;
			}

			const drawSurfInstance_t* instances = drawGroup[ passIx ].Instances();
			for( uint32_t surfIx = 0; surfIx < drawGroup[ passIx ].InstanceCount(); ++surfIx )
			{
				const uint32_t instanceId = drawGroupOffset[ passIx ] + drawGroup[ passIx ].InstanceId( surfIx );
				m_surfBuffer[ instanceId ].prevModel = ( currentFrame > 0 ) ? m_surfBuffer[ instanceId ].model : instances[ surfIx ].modelMatrix.Transpose();
				m_surfBuffer[ instanceId ].model = instances[ surfIx ].modelMatrix.Transpose();				
				m_surfBuffer[ instanceId ].diffuseIblCubeId = instances[ surfIx ].diffuseIblId;
				m_surfBuffer[ instanceId ].envCubeId = instances[ surfIx ].envMapId;
			}
		}

		m_surfParmeters.SetPos( 0 );
		m_surfParmeters.CopyData( m_surfBuffer, sizeof( gpuSurface_t ) * MaxSurfaces );
	}

	// Bindings
	if( m_lastUpdateFrame != currentFrame )
	{
		m_viewParms->Bind( BINDING_NAME( modelBuffer ), &m_surfParmeters );

		m_lastUpdateFrame = currentFrame;
	}

	// Draw Passes
	for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
	{
		for ( uint32_t passIx = begin; passIx <= end; ++passIx )
		{
			DrawPass* pass = passes[ multiViewIndex ][ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			pass->FrameBegin( m_resources );
		}
	}
}


void RenderView::FrameEnd( const drawPass_t begin, const drawPass_t end )
{
	for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
	{
		m_previousViewProjMatrices[ multiViewIndex ] = m_viewProjMatrices[ multiViewIndex ];

		for ( uint32_t passIx = begin; passIx <= end; ++passIx )
		{
			DrawPass* pass = passes[ multiViewIndex ][ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			pass->FrameEnd();
		}
	}
}


bool RenderView::NeedsResize() const
{
	return ( m_context == nullptr ) || ( m_context->FrameNumber() != m_lastResizeFrame );
}


void RenderView::Resize()
{
	if( NeedsResize() == false ) {
		return;
	}

	CreateFrameBuffers( m_fbSourceImages );

	for ( uint32_t multiViewIndex = 0; multiViewIndex < m_multiViewCount; ++multiViewIndex )
	{
		m_framebuffers[ multiViewIndex ]->Resize();
		for ( uint32_t passIx = 0; passIx < DRAWPASS_COUNT; ++passIx )
		{
			DrawPass* pass = passes[ multiViewIndex ][ passIx ];
			if ( pass == nullptr ) {
				continue;
			}
			pass->Resize();
		}
	}
	SetViewRect( 0, 0, m_framebuffers[ 0 ]->GetWidth(), m_framebuffers[ 0 ]->GetHeight() );

	m_lastResizeFrame = m_context->FrameNumber();
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


bool RenderView::Finalize() const
{
	return m_finalizeImage;
}


bool RenderView::Clear() const
{
	return m_clearImage;
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


const mat4x4f& RenderView::GetViewProjMatrix( const uint32_t multiView ) const
{
	return m_viewProjMatrices[ multiView ];
}


const mat4x4f& RenderView::GetPreviousViewProjMatrix( const uint32_t multiView ) const
{
	return m_previousViewProjMatrices[ multiView ];
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
	m_viewProjMatrices[ multiView ] = m_projMatrices[ multiView ] * m_viewMatrices[ multiView ];

	m_viewport.near = camera.GetNearClip();
	m_viewport.far = camera.GetFarClip();

	m_viewOrigin = camera.GetOrigin().xyz;
}

void RenderView::SetCamera2D( const Camera& camera, const vec4f& frame, const uint32_t multiView )
{
	m_viewMatrices[ multiView ] = mat4x4f::Identity();
#if USE_OPENGL_CONVENTIONS
	m_projMatrices[ multiView ] = camera.GetOrthographicMatrix( frame[ 0 ], frame[ 1 ], frame[ 3 ], frame[ 2 ] );
#else
	m_projMatrices[ multiView ] = camera.GetOrthographicMatrix( frame[ 0 ], frame[ 1 ], frame[ 2 ], frame[ 3 ] );
#endif
	m_viewProjMatrices[ multiView ] = m_projMatrices[ multiView ] * m_viewMatrices[ multiView ];

	m_viewport.near = camera.GetNearClip();
	m_viewport.far = camera.GetFarClip();

	m_viewOrigin = camera.GetOrigin().xyz;
}

void RenderView::AttachDebugMenu( const debugMenuFuncPtr funcPtr )
{
	debugMenus.Append( funcPtr );
}
