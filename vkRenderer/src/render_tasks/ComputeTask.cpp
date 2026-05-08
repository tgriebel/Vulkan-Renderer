#include "ComputeTask.h"

#include "../render_resources/gpuBuffer.h"
#include "../render_binding/bindings.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/assetLib.h"


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
	m_progName = info.progName;

	m_dispatchX = info.dispatchX;
	m_dispatchY = info.dispatchY;
	m_dispatchZ = info.dispatchZ;

	m_resourceImage = info.resourceImage;
	m_outputBuffer = info.outputBuffer;

	m_imageArray.SetRenderContext( m_context );
	if ( m_resourceImage != nullptr )
	{
		m_imageArray.Resize( 1 );
		m_imageArray.BindIndex( 0, m_resourceImage );
	}

	m_hasParmBuffer = ( info.parmBufferData != nullptr && info.parmBufferSize > 0 );
	if ( m_hasParmBuffer )
	{
		m_parmBuffer.Create(
			"Compute Parms",
			swapBuffering_t::SINGLE_FRAME,
			resourceLifeTime_t::TASK,
			1,
			info.parmBufferSize,
			bufferType_t::UNIFORM
		);
		m_parmBuffer.SetPos( 0 );
		m_parmBuffer.CopyData( info.parmBufferData, info.parmBufferSize );
	}

	if ( info.pushConstants != nullptr && info.pushConstantsSize > 0 )
	{
		m_pushConstants.resize( info.pushConstantsSize );
		memcpy( m_pushConstants.data(), info.pushConstants, info.pushConstantsSize );
	}

	m_parms = m_context->RegisterBindParm( m_context->LookupBindSet( bindset_compute ) );
}


void ComputeTask::FrameBegin()
{
	m_parms->Bind( BINDING_NAME( globalsBuffer ), &m_resources->globalConstants );
	m_parms->Bind( BINDING_NAME( computeImage ), &m_imageArray );
	m_parms->Bind( BINDING_NAME( computeParms ), &m_parmBuffer );
	m_parms->Bind( BINDING_NAME( computeWrite ), m_outputBuffer );
}


void ComputeTask::Execute( CommandList& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( ColorLGrey ) );

	const hdl_t progHdl = AssetLib<GpuProgram>::Handle( m_progName.c_str() );

	if ( m_pushConstants.empty() ) {
		cmdContext.Dispatch( progHdl, *m_parms, m_dispatchX, m_dispatchY, m_dispatchZ );
	} else {
		cmdContext.Dispatch( progHdl, *m_parms, m_pushConstants.data(), static_cast<uint32_t>( m_pushConstants.size() ), m_dispatchX, m_dispatchY, m_dispatchZ );
	}

	cmdContext.MarkerEndRegion();
}


void ComputeTask::Shutdown()
{
}
