#include "renderResource.h"
#include "../render_state/cmdContext.h"
#include "../render_core/gpuImage.h"

static std::vector<RenderResource*> m_taskDependentResources;
static std::vector<RenderResource*> m_frameDependentResources;
static std::vector<RenderResource*> m_viewDependentResources;
static std::vector<RenderResource*> m_appDependentResources;

std::vector<RenderResource*> RenderResource::GetResourceList( const resourceLifeTime_t lifetime )
{
	switch ( lifetime )
	{
	case resourceLifeTime_t::TASK:	return m_taskDependentResources;
	case resourceLifeTime_t::FRAME:	return m_frameDependentResources;
	case resourceLifeTime_t::RESIZE:	return m_viewDependentResources;
	case resourceLifeTime_t::REBOOT:	return m_appDependentResources;
	}
	return std::vector<RenderResource*>();
}


void RenderResource::Cleanup( const resourceLifeTime_t lifetime )
{
	if ( lifetime == resourceLifeTime_t::TASK )
	{
		const uint32_t resourceCount = static_cast<uint32_t>( m_taskDependentResources.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			m_taskDependentResources[ i ]->Destroy();
		}
		m_taskDependentResources.clear();
	}
	else if ( lifetime == resourceLifeTime_t::FRAME )
	{
		const uint32_t resourceCount = static_cast<uint32_t>( m_frameDependentResources.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			m_frameDependentResources[ i ]->Destroy();
		}
		m_frameDependentResources.clear();
	}
	else if ( lifetime == resourceLifeTime_t::RESIZE )
	{
		const uint32_t resourceCount = static_cast<uint32_t>( m_viewDependentResources.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			m_viewDependentResources[ i ]->Destroy();
		}
		m_viewDependentResources.clear();
	}
	else if ( lifetime == resourceLifeTime_t::REBOOT )
	{
		const uint32_t resourceCount = static_cast<uint32_t>( m_appDependentResources.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			m_appDependentResources[ i ]->Destroy();
		}
		m_appDependentResources.clear();
	}
}


void RenderResource::TransitionImages( CommandContext* cmdCommand, const resourceLifeTime_t lifetime )
{
	std::vector<RenderResource*>* resourceList = nullptr;
	if ( lifetime == resourceLifeTime_t::REBOOT ) {
		resourceList = &m_appDependentResources;
	} else if ( lifetime == resourceLifeTime_t::RESIZE ) {
		resourceList = &m_viewDependentResources;
	} else if ( lifetime == resourceLifeTime_t::TASK ) {
		resourceList = &m_taskDependentResources;
	}

	if( resourceList == nullptr ) {
		return;
	}

	const uint32_t resourceCount = static_cast<uint32_t>( resourceList->size() );
	for ( uint32_t i = 0; i < resourceCount; ++i )
	{
		if ( (*resourceList)[ i ]->m_type == resourceType_t::GPU_IMAGE )
		{
			GpuImage* gpuImage = reinterpret_cast<GpuImage*>( ( *resourceList )[ i ] );
			Transition( cmdCommand, gpuImage, swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_READ );
		}
	}
}


void RenderResource::Create( const resourceType_t type, const resourceLifeTime_t lifetime )
{
	m_type = type;
	m_lifetime = lifetime;

	m_resourceMemoryRegion = memoryRegion_t::UNKNOWN;
	m_resourceByteCount = 0;

	switch ( m_lifetime )
	{
	case resourceLifeTime_t::TASK:		m_taskDependentResources.push_back( this );		break;
	case resourceLifeTime_t::FRAME:		m_frameDependentResources.push_back( this );	break;
	case resourceLifeTime_t::RESIZE:	m_viewDependentResources.push_back( this );		break;
	case resourceLifeTime_t::REBOOT:	m_appDependentResources.push_back( this );		break;
	}
}