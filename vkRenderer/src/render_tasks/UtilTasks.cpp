#include "UtilTasks.h"
#include "imageShaderTask.h"
#include "../render_state/cmdContext.h"
#include "../globals/assetDefs.h"
#include "../render_core/renderer.h"


// ResolveImageTask

ResolveImageTask::ResolveImageTask( const resolveTaskCreateInfo_t& createInfo )
{
	m_count      = Min( createInfo.count, MaxResolves );
	m_createInfo = createInfo;
	m_context    = createInfo.context;
	m_resources  = createInfo.resources;

	InitShaderTasks();
}


void ResolveImageTask::InitShaderTasks()
{
	for( uint32_t i = 0; i < m_count; ++i )
	{
		if( m_createInfo.resolves[ i ].useApi )
		{
			m_shaderTasks[ i ] = nullptr;
			continue;
		}

		Image* srcImage = m_createInfo.resolves[ i ].info.src;
		Image* dstImage = m_createInfo.resolves[ i ].info.dst;

		const bool isDepth = IsDepthStencilCompatible( srcImage->info.fmt );

		if( isDepth )
		{
			imageShaderCreateInfo_t info = {};
			info.name = "ResolveDepthSubTask";
			info.clear = false;
			info.progHdl = AssetLibGpuProgram::Handle( "ResolveDepth" );
			info.permSet = shaderPermId_t::NONE;
			info.outputImage = dstImage;
			info.context = m_context;
			info.resources = m_resources;
			info.inputImages = 2;

			m_shaderTasks[ i ] = new ImageShaderTask( info );

			// FIXME: needs to be made locally or supplied
			m_shaderTasks[ i ]->SetSourceImage( 0, &m_resources->depthImageView );
			m_shaderTasks[ i ]->SetSourceImage( 1, &m_resources->stencilImageView );
		}
		else
		{
			imageShaderCreateInfo_t info = {};
			info.name = "ResolveColorSubTask";
			info.clear = false;
			info.progHdl = AssetLibGpuProgram::Handle( "Resolve" );
			info.permSet = ForceDisableMSAA ? shaderPermId_t::NONE : shaderPermId_t::MSAA;
			info.outputImage = dstImage;
			info.context = m_context;
			info.resources = m_resources;
			info.inputImages = 1;

			m_shaderTasks[ i ] = new ImageShaderTask( info );

			m_shaderTasks[ i ]->SetSourceImage( 0, srcImage );
		}
	}
}


ResolveImageTask::~ResolveImageTask()
{
	for ( uint32_t i = 0; i < m_count; ++i )
	{
		delete m_shaderTasks[ i ];
		m_shaderTasks[ i ] = nullptr;
	}
}


std::string ResolveImageTask::AsString() const
{
	std::stringstream ss;
	ss << "ResolveImageTask: " << m_count << " resolve(s)";
	for ( uint32_t i = 0; i < m_count; ++i )
	{
		ss << " [" << m_createInfo.resolves[ i ].info.src->gpuImage->GetDebugName()
		   << " -> " << m_createInfo.resolves[ i ].info.dst->gpuImage->GetDebugName() << "]";
	}
	return ss.str();
}


void ResolveImageTask::FrameBegin()
{
	for ( uint32_t i = 0; i < m_count; ++i )
	{
		if ( m_shaderTasks[ i ] != nullptr ) {
			m_shaderTasks[ i ]->FrameBegin();
		}
	}
	GpuTask::OnFrameBegin();
}


void ResolveImageTask::FrameEnd()
{
	for ( uint32_t i = 0; i < m_count; ++i )
	{
		if ( m_shaderTasks[ i ] != nullptr ) {
			m_shaderTasks[ i ]->FrameEnd();
		}
	}
}


void ResolveImageTask::Resize()
{
	for ( uint32_t i = 0; i < m_count; ++i )
	{
		if ( m_shaderTasks[ i ] != nullptr ) {
			m_shaderTasks[ i ]->Resize();
		}
	}
}


void ResolveImageTask::Execute( CommandList& context )
{
	for ( uint32_t i = 0; i < m_count; ++i )
	{
		if( m_createInfo.resolves[ i ].useApi )
		{
			ResolveImage( &context, m_createInfo.resolves[ i ].info );
		}
		else
		{
			assert( m_shaderTasks[ i ] != nullptr );
			m_shaderTasks[ i ]->Execute( context );
		}
	}
}


// TransitionImageTask

std::string TransitionImageTask::AsString() const
{
	std::stringstream ss;
	ss << "TransitionImageTask: " << m_img->gpuImage->GetDebugName();
	return ss.str();
}


void TransitionImageTask::Execute( CommandList& context )
{
	Transition( &context, *m_img, m_srcState, m_dstState );
}


// CopyImageTask

CopyImageTask::CopyImageTask( Image* src, Image* dst )
{
	m_src = src;
	m_dst = dst;

	m_srcParms.baseArray  = 0;
	m_srcParms.arrayCount = m_src->subResourceView.arrayCount;
	m_srcParms.baseMip    = 0;
	m_srcParms.mipLevels  = m_src->subResourceView.mipLevels;
	m_srcParms.x          = 0;
	m_srcParms.y          = 0;
	m_srcParms.z          = 0;
	m_srcParms.width      = m_src->info.width;
	m_srcParms.height     = m_src->info.height;
	m_srcParms.depth      = 1;

	m_dstParms.baseArray  = 0;
	m_dstParms.arrayCount = m_dst->subResourceView.arrayCount;
	m_dstParms.baseMip    = 0;
	m_dstParms.mipLevels  = m_dst->subResourceView.mipLevels;
	m_dstParms.x          = 0;
	m_dstParms.y          = 0;
	m_dstParms.z          = 0;
	m_dstParms.width      = m_dst->info.width;
	m_dstParms.height     = m_dst->info.height;
	m_dstParms.depth      = 1;
}


CopyImageTask::CopyImageTask( Image* src, const copyImageParms_t& srcParms, Image* dst, const copyImageParms_t& dstParms )
{
	m_src      = src;
	m_dst      = dst;
	m_srcParms = srcParms;
	m_dstParms = dstParms;
}


std::string CopyImageTask::AsString() const
{
	std::stringstream ss;
	ss << "CopyImageTask: " << m_src->gpuImage->GetDebugName() << " -> " << m_dst->gpuImage->GetDebugName();
	return ss.str();
}


void CopyImageTask::SetSourceParms( const copyImageParms_t& src )
{
	m_srcParms = src;
}


void CopyImageTask::SetDestinationParms( const copyImageParms_t& dst )
{
	m_dstParms = dst;
}


void CopyImageTask::Execute( CommandList& context )
{
	CopyImage( &context, *m_src, m_srcParms, *m_dst, m_dstParms );
}
