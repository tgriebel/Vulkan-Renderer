#include "UtilTasks.h"
#include "../render_state/cmdContext.h"
#include "../globals/assetDefs.h"
#include "../render_core/renderer.h"


// ComputeTask

ComputeTask::ComputeTask( const char* csName, ComputeState* state )
{
	m_name    = csName;
	m_state   = state;
	m_progHdl = GpuProgramLib().RetrieveHdl( csName );
}


std::string ComputeTask::AsString() const
{
	std::stringstream ss;
	ss << "<ComputeTask: " << m_name << ">";
	return ss.str();
}


void ComputeTask::Execute( CommandContext& context )
{
	ComputeCmdList* computeContext = reinterpret_cast<ComputeCmdList*>( &context );
	computeContext->Dispatch( m_progHdl, *m_state->parms, m_state->x, m_state->y, m_state->z );
}


// ResolveImageTask

std::string ResolveImageTask::AsString() const
{
	std::stringstream ss;
	ss << "<ResolveImageTask: " << m_info.src->gpuImage->GetDebugName() << " -> " << m_info.dst->gpuImage->GetDebugName() << ">";
	return ss.str();
}


void ResolveImageTask::Execute( CommandContext& context )
{
	ResolveImage( &context, m_info );
}


// TransitionImageTask

std::string TransitionImageTask::AsString() const
{
	std::stringstream ss;
	ss << "<TransitionImageTask: " << m_img->gpuImage->GetDebugName() << ">";
	return ss.str();
}


void TransitionImageTask::Execute( CommandContext& context )
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
	ss << "<CopyImageTask: " << m_src->gpuImage->GetDebugName() << " -> " << m_dst->gpuImage->GetDebugName() << ">";
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


void CopyImageTask::Execute( CommandContext& context )
{
	CopyImage( &context, *m_src, m_srcParms, *m_dst, m_dstParms );
}
