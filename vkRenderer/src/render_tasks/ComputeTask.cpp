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
	m_dispatchByTile = ( info.dispatchX == 0 ) || ( info.dispatchY == 0 ) || ( info.dispatchZ == 0 );

	m_context = info.context;
	m_resources = info.resources;
	m_name = info.name;

	if( m_dispatchByTile == false )
	{
		m_imageTileSizeX = 0;
		m_imageTileSizeY = 0;
		m_image = nullptr;

		m_dispatchX = info.dispatchX;
		m_dispatchY = info.dispatchY;
		m_dispatchZ = info.dispatchZ;
	}
	else
	{
		assert( info.image != nullptr );
		assert( info.imageTileSizeX != 0 );
		assert( info.imageTileSizeY != 0 );

		m_imageTileSizeX = info.imageTileSizeX;
		m_imageTileSizeY = info.imageTileSizeY;
		m_image = info.image;

		m_dispatchX = 0;
		m_dispatchY = 0;
		m_dispatchZ = 0;
	}

	m_bind = info.bind;

	m_shaderConstantsByteSize = 0;
	if( ( info.constants != nullptr ) && ( info.constantsByteSize > 0 ) )
	{
		m_shaderConstantsByteSize = Min( info.constantsByteSize, MaxCustomConstantBytes );
		memcpy( m_shaderConstants, info.constants, m_shaderConstantsByteSize );
	}

	m_progAsset = GpuProgramLib().Find( info.progName );

	CreateComputePipeline( *m_progAsset );

	m_parms = m_context->RegisterBindParm( m_name, m_context->LookupBindSet( info.bindSetId ) );
}


void ComputeTask::FrameBegin()
{
	if ( m_bind ) {
		m_bind( this, m_parms );
	}
	GpuTask::OnFrameBegin();
}


void ComputeTask::Execute( CommandList& cmdContext )
{
	cmdContext.MarkerBeginRegion( m_name.c_str(), ColorToVector( ColorLGrey ) );

	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;

	if( m_dispatchByTile )
	{
		x = ComputeCmdList::DispatchDim( m_image->info.width, m_imageTileSizeX );
		y = ComputeCmdList::DispatchDim( m_image->info.height, m_imageTileSizeY );
		z = 1;
	}
	else
	{
		x = m_dispatchX;
		y = m_dispatchY;
		z = m_dispatchZ;
	}

	// TODO: Add logic to add this to buffer if needed
	assert( m_shaderConstantsByteSize < MaxCustomPushConstantBytes );

	if ( HasConstants() ) {
		cmdContext.Dispatch( *m_progAsset, *m_parms, m_shaderConstants, m_shaderConstantsByteSize, x, y, z );	
	} else {
		cmdContext.Dispatch( *m_progAsset, *m_parms, x, y, z );
	}

	cmdContext.MarkerEndRegion();
}


void ComputeTask::Shutdown()
{
}
