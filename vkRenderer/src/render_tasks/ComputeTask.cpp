#include "ComputeTask.h"

#include "../render_resources/gpuBuffer.h"
#include "../render_binding/bindings.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/assetLib.h"
#include "../globals/assetDefs.h"

std::string ComputeTask::AsString() const
{
	std::stringstream ss;
	ss << "ComputeTask: " << m_name;
	return ss.str();
}


void ComputeTask::Init( const computeTaskCreateInfo_t& info )
{
	m_context = info.context;
	m_resources = info.resources;
	m_name = info.name;

	m_dispatchX = info.dispatchX;
	m_dispatchY = info.dispatchY;
	m_dispatchZ = info.dispatchZ;

	m_bind = info.bind;

	if ( info.pushConstants != nullptr && info.pushConstantsSize > 0 )
	{
		m_pushConstants.resize( info.pushConstantsSize );
		memcpy( m_pushConstants.data(), info.pushConstants, info.pushConstantsSize );
	}

	m_progHdl = AssetLib<GpuProgram>::Handle( info.progName );
	Asset<GpuProgram>* prog = GpuProgramLib().Find( m_progHdl );

	CreateComputePipeline( *prog );

	m_parms = m_context->RegisterBindParm( m_context->LookupBindSet( info.bindSetId ) );
}


void ComputeTask::FrameBegin()
{
	if ( m_bind ) {
		m_bind( m_parms );
	}
}


void ComputeTask::Execute( CommandList& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( ColorLGrey ) );

	if ( m_pushConstants.empty() ) {
		cmdContext.Dispatch( m_progHdl, *m_parms, m_dispatchX, m_dispatchY, m_dispatchZ );
	} else {
		cmdContext.Dispatch( m_progHdl, *m_parms, m_pushConstants.data(), static_cast<uint32_t>( m_pushConstants.size() ), m_dispatchX, m_dispatchY, m_dispatchZ );
	}

	cmdContext.MarkerEndRegion();
}


void ComputeTask::Shutdown()
{
}
