#include "imageShaderTask.h"

#include "../scene/sceneBase.h"
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"
#include "../draw_passes/postPass.h"
#include "../globals/assetDefs.h"

std::string ImageShaderTask::AsString() const
{
	std::stringstream ss;
	ss << "ImageShaderTask: " << m_dbgName;
	return ss.str();
}


void ImageShaderTask::Init( const imageShaderCreateInfo_t& info )
{
	SCOPED_TIMER_PRINT( ImageProcessInit, MILLISECOND )

	m_dbgName = info.name;
	m_layer = info.layer;
	m_mipLevel = info.mipLevel;

	m_resources = info.resources;
	m_context = info.context;

	m_viewId = info.viewId;

	Image* outputImage0 = info.outputImage;
	Image* outputImage1 = info.outputImage1;
	Image* outputImage2 = info.outputImage2;

	m_taskImageCount = info.taskImageCount;

	if( m_taskImageCount > 0 )
	{
		assert( ( outputImage0 == nullptr ) && ( outputImage1 == nullptr ) && ( outputImage2 == nullptr ) );

		for( uint32_t i = 0; i < m_taskImageCount; ++i )
		{
			m_taskImages[ i ] = new Image(
				info.createInfos[ i ],
				"ImageShaderImage", GPU_IMAGE_RW, resourceLifeTime_t::TASK
			);
		}
		outputImage0 = m_taskImages[ 0 ];
		outputImage1 = m_taskImages[ 1 ];
		outputImage2 = m_taskImages[ 2 ];
	}

	// Set-up Views and Frame Buffers
	{
		imageSubResourceView_t view{};
		view.baseArray = m_layer;
		view.arrayCount = 1;
		view.baseMip = m_mipLevel;
		view.mipLevels = 1;
		view.aspect = GetColorAspectFlags( outputImage0->info.fmt );

		imageInfo_t imageInfo = outputImage0->info;
		imageInfo.type = IMAGE_TYPE_2D;

		assert( info.passCount <= MaxPasses );
		m_passCount = Clamp( info.passCount, 1u, MaxPasses );

		uint32_t passIndex = 0;

		// Intermediate Frame Buffer
		if ( m_passCount > 1 )
		{
			m_outputImageViews[ passIndex ][ 0 ] = new ImageView( info.resources->tempColorImage, imageInfo, view, resourceLifeTime_t::RESIZE );

			assert( ( m_outputImageViews[ passIndex ][ 1 ] == nullptr ) && ( m_outputImageViews[ passIndex ][ 2 ] == nullptr ) ); // Not supported

			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = "TempImageProcessFb";
			fbInfo.color0 = m_outputImageViews[ passIndex ][ 0 ];
			fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME; 
			fbInfo.context = m_context;

			m_fb[ passIndex ].Create( fbInfo );
			m_passes[ passIndex ] = new PostPass( m_context, &m_fb[ passIndex ] );

			++passIndex;
		}

		// Main Frame Buffer
		{
			m_image = outputImage0;
			m_outputImageViews[ passIndex ][ 0 ] = new ImageView( m_image, imageInfo, view, resourceLifeTime_t::RESIZE );

			m_outputImageViews[ passIndex ][ 1 ] = nullptr;
			m_outputImageViews[ passIndex ][ 2 ] = nullptr;

			if( outputImage1 )
			{
				imageInfo_t imageInfo = outputImage1->info;
				imageInfo.type = IMAGE_TYPE_2D;

				m_outputImageViews[ passIndex ][ 1 ] = new ImageView( outputImage1, imageInfo, view, resourceLifeTime_t::RESIZE );
			}

			if ( outputImage2 )
			{
				imageInfo_t imageInfo = outputImage2->info;
				imageInfo.type = IMAGE_TYPE_2D;

				m_outputImageViews[ passIndex ][ 2 ] = new ImageView( outputImage2, imageInfo, view, resourceLifeTime_t::RESIZE );
			}

			frameBufferCreateInfo_t fbInfo;
			fbInfo.name = m_dbgName.c_str();
			fbInfo.context = m_context;
			fbInfo.color0 = m_outputImageViews[ passIndex ][ 0 ];
			fbInfo.color1 = m_outputImageViews[ passIndex ][ 1 ];
			fbInfo.color2 = m_outputImageViews[ passIndex ][ 2 ];
			fbInfo.swapBuffering = swapBuffering_t::SINGLE_FRAME;

			m_fb[ passIndex ].Create( fbInfo );
			m_passes[ passIndex ] = new PostPass( m_context, &m_fb[ passIndex ] );

			++passIndex;
		}
	}

	m_clearColor = vec4f( 0.0f, 0.5f, 0.5f, 1.0f );

	m_transitionState = {};
	m_transitionState.flags.clear = info.clear;
	m_transitionState.flags.store = true;
	m_transitionState.flags.presentAfter = info.present;
	m_transitionState.flags.readAfter = !info.present;
	m_transitionState.flags.readBefore = true;

	assert( info.progHdl != INVALID_HDL );
	m_progAsset = GpuProgramLib().Find( info.progHdl );
	m_permSet = info.permSet;

	uint32_t multiPassImageCount = ( m_passCount > 1 ) ? 1 : 0;

	m_image2dSlotCount = info.inputImages;
	m_imageCubeSlotCount = info.inputCubeImages;

	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{		
		m_passes[ passIndex ]->codeImages.SetRenderContext( m_context );
		m_passes[ passIndex ]->codeCubeImages.SetRenderContext( m_context );

		m_passes[ passIndex ]->codeImages.Resize( info.inputImages + multiPassImageCount );
		m_passes[ passIndex ]->codeCubeImages.Resize( info.inputCubeImages );

		const uint32_t codeImageCount = m_passes[ passIndex ]->codeImages.Count();
		for( uint32_t codeImageIx = 0; codeImageIx < codeImageCount; ++codeImageIx ) {
			m_passes[ passIndex ]->codeImages.BindIndex( codeImageIx, rc.defaultImage );
		}

		m_passes[ passIndex ]->parms = m_context->RegisterBindParm( bindset_imageShader );

		m_buffer[ passIndex ].Create( "Resource buffer", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::TASK, 1, MaxBufferSizeInBytes, bufferType_t::UNIFORM );
	}

	// HACK: Clean this up with a better design
	// Attach the previous frame buffers as input for the next pass
	for( uint32_t passIndex = 1; passIndex < m_passCount; ++passIndex )
	{
		PostPass* currentPass = m_passes[ passIndex ];

		const ImageView* lastPassOutputColorImage = m_outputImageViews[ passIndex - 1 ][ 0 ];

		const uint32_t codeImageCount = currentPass->codeImages.Count();
		currentPass->codeImages.BindIndex( codeImageCount - 1, lastPassOutputColorImage );
	}
}


ImageView* ImageShaderTask::GetOutputImage( const uint32_t outputImageIndex )
{
	return m_outputImageViews[ m_passCount - 1 ][ outputImageIndex ];
}


void ImageShaderTask::SetSourceImage( const uint32_t slot, Image* image )
{
	assert( image->info.type == imageType_t::IMAGE_TYPE_2D );
	assert( m_image2dSlotCount > slot );

	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{
		if( slot < m_image2dSlotCount ) {
			m_passes[ passIndex ]->codeImages.BindIndex( slot, image );
		}
	}
}


void ImageShaderTask::SetSourceCubeImage( const uint32_t slot, Image* image )
{
	assert( image->info.type == imageType_t::IMAGE_TYPE_CUBE );
	assert( m_imageCubeSlotCount > slot );

	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{
		if ( slot < m_imageCubeSlotCount ) {
			m_passes[ passIndex ]->codeCubeImages.BindIndex( slot, image );
		}
	}
}


void ImageShaderTask::UpdateConstants( const void* dataBlock, const uint32_t sizeInBytes )
{
	assert( sizeInBytes <= MaxConstantBlockSizeInBytes );

	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{	
		m_buffer[ passIndex ].SetPos( ReservedConstantSizeInBytes );
		m_buffer[ passIndex ].CopyData( dataBlock, Min( sizeInBytes, MaxConstantBlockSizeInBytes ) );
	}
}


void ImageShaderTask::Resize()
{
	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex ) {
		m_passes[ passIndex ]->Resize();
	}
}


void ImageShaderTask::Shutdown()
{
	for ( uint32_t i = 0; i < MaxOutputImages; ++i )
	{
		if( m_taskImages[ i ] != nullptr )
		{
			// FIXME: gpu images leak CPU memory b/c ownership was never established
			// FIXME: dependency issue with TASK resource cleanup
			delete m_taskImages[ i ];
			m_taskImages[ i ] = nullptr;
		}
	}

	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{
		for ( uint32_t outputImageIx = 0; outputImageIx < MaxOutputImages; ++outputImageIx )
		{
			if ( m_outputImageViews[ passIndex ][ outputImageIx ] != nullptr )
			{
				delete m_outputImageViews[ passIndex ][ outputImageIx ];
				m_outputImageViews[ passIndex ][ outputImageIx ] = nullptr;
			}
		}

		m_buffer[ passIndex ].Destroy();

		m_fb[ passIndex ].Destroy();

		if ( m_passes[ passIndex ] != nullptr )
		{
			delete m_passes[ passIndex ];
			m_passes[ passIndex ] = nullptr;
		}
	}
}


void ImageShaderTask::FrameBegin()
{
	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{
		// Set standard constants
		{
			const viewport_t& viewport = m_passes[ passIndex ]->GetViewport();
			const float w = float( viewport.width );
			const float h = float( viewport.height );

			baseConstants_t constants {};
			constants.dimensions = vec4f( w, h, 1.0f / w, 1.0f / h );
			constants.pass = passIndex;
			constants.previousImageId = m_image2dSlotCount;
			constants.pad0 = m_viewId;
			constants.level = m_mipLevel;
			constants.layer = m_layer;
			constants.mipCount = m_image->info.mipLevels;
			constants.layerCount = m_image->info.layers;

			const uint64_t offset = m_buffer[ passIndex ].GetSize();
			m_buffer[ passIndex ].SetPos( 0 );
			m_buffer[ passIndex ].CopyData( &constants, sizeof( baseConstants_t ) );
			m_buffer[ passIndex ].SetPos( offset );
		}

		// Set standard binds
		m_passes[ passIndex ]->parms->Bind( BINDING_NAME( sourceImages ),		m_passes[ passIndex ]->codeImages.Count() > 0 ? &m_passes[ passIndex ]->codeImages : &rc.defaultImageArray );
		m_passes[ passIndex ]->parms->Bind( BINDING_NAME( sourceCubeImages ),	m_passes[ passIndex ]->codeCubeImages.Count() > 0 ? m_passes[ passIndex ]->codeCubeImages[ 0 ] : rc.defaultImageCube );
		m_passes[ passIndex ]->parms->Bind( BINDING_NAME( imageStencil ),		&m_resources->stencilImageView ); // FIXME: allow either special desc sets or null inputs
		m_passes[ passIndex ]->parms->Bind( BINDING_NAME( imageProcess ),		&m_buffer[ passIndex ] );
	}

	// std::cout << m_pass->parms->AsString() << std::endl;

	GpuTask::OnFrameBegin();
}


void ImageShaderTask::FrameEnd()
{

}


void ImageShaderTask::Execute( CommandList& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_dbgName.c_str(), ColorToVector( Color::Brown ) );

	for ( uint32_t passIndex = 0; passIndex < m_passCount; ++passIndex )
	{
		if( m_passCount > 1 ) {
			cmdContext.MarkerBeginRegion( ( passIndex == 0 ) ? "Pass #0" : "Pass #1", ColorToVector( Color::White ) );
		}

		m_passes[ passIndex ]->InsertResourceBarriers( cmdContext );

		hdl_t pipeLineHandle = CreateGraphicsPipeline( m_passes[ passIndex ], *m_progAsset, m_permSet );

		vk_RenderImageShader( cmdContext, pipeLineHandle, m_passes[ passIndex ], m_transitionState );

		if ( m_passCount > 1 ) {
			cmdContext.MarkerEndRegion();
		}
	}

	cmdContext.MarkerEndRegion();
}
