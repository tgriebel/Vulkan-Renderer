#include "renderResource.h"
#include "../render_state/cmdContext.h"
#include "../render_core/gpuImage.h"
#include <algorithm>
#include "../app/window.h"

extern Window g_window;

static std::vector<RenderResource*> m_taskDependentResources;
static std::vector<RenderResource*> m_frameDependentResources;
static std::vector<RenderResource*> m_viewDependentResources;
static std::vector<RenderResource*> m_appDependentResources;

std::vector<RenderResource*> RenderResource::GetResourceList( const resourceLifeTime_t lifetime )
{
	switch ( lifetime )
	{
	case resourceLifeTime_t::TASK:		return m_taskDependentResources;
	case resourceLifeTime_t::FRAME:		return m_frameDependentResources;
	case resourceLifeTime_t::RESIZE:	return m_viewDependentResources;
	case resourceLifeTime_t::REBOOT:	return m_appDependentResources;
	}
	return std::vector<RenderResource*>();
}


void RenderResource::ResizeResources( const uint32_t displayWidth, const uint32_t displayHeight )
{
	// This is tricky b/c recreating new resources adds to this exact list! Uh-oh! No worries though!
	// The solution is to use a move which clears the global list while providing a local copy
	std::vector<RenderResource*> resizeList = std::move( m_viewDependentResources );

	const uint32_t resourceCount = static_cast<uint32_t>( resizeList.size() );
	for( uint32_t i = 0; i < resourceCount; ++i )
	{
		resizeList[ i ]->OnResize( displayWidth, displayHeight );
	}
}


void RenderResource::Cleanup( const resourceLifeTime_t lifetime )
{
	if ( lifetime == resourceLifeTime_t::TASK )
	{
		std::vector<RenderResource*> taskList = std::move( m_taskDependentResources );
		const uint32_t resourceCount = static_cast<uint32_t>( taskList.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			taskList[ i ]->Destroy();
		}
	}
	else if ( lifetime == resourceLifeTime_t::FRAME )
	{
		std::vector<RenderResource*> frameList = std::move( m_frameDependentResources );
		const uint32_t resourceCount = static_cast<uint32_t>( frameList.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			frameList[ i ]->Destroy();
		}
	}
	else if ( lifetime == resourceLifeTime_t::RESIZE )
	{
		std::vector<RenderResource*> viewList = std::move( m_viewDependentResources );
		const uint32_t resourceCount = static_cast<uint32_t>( viewList.size() );
		for( uint32_t i = 0; i < resourceCount; ++i ) {
			viewList[ i ]->Destroy();
		}
	}
	else if ( lifetime == resourceLifeTime_t::REBOOT )
	{
		std::vector<RenderResource*> appList = std::move( m_appDependentResources );
		const uint32_t resourceCount = static_cast<uint32_t>( appList.size() );
		for ( uint32_t i = 0; i < resourceCount; ++i ) {
			appList[ i ]->Destroy();
		}
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


static void InsertSorted( std::vector<RenderResource*>& list, RenderResource* resource )
{
	auto it = std::lower_bound( list.begin(), list.end(), resource,
		[]( const RenderResource* a, const RenderResource* b ) {
			return a->GetPriority() < b->GetPriority();
		} );
	list.insert( it, resource );
}


static resourcePriority_t GetPriorityForType( const resourceType_t type )
{
	// Defines a dependency hierachy from most fundamental to least
	// Destructures should reverse the priority
	// Resize bascially needs this most since GpuImage, ImageView, and FrameBuffer build off Image class
	switch( type )
	{
	case resourceType_t::MEMORY:		return resourcePriority_t::HIGHEST; break;
	case resourceType_t::BUFFER:		return resourcePriority_t::HIGHEST; break;
	case resourceType_t::FB_IMAGE:		return resourcePriority_t::HIGHEST; break;	// Sets the image info, most important piece for resizing
	case resourceType_t::GPU_IMAGE:		return resourcePriority_t::MEDIUM; break;
	case resourceType_t::SWAPCHAIN:		return resourcePriority_t::MEDIUM; break;
	case resourceType_t::IMAGE_VIEW:	return resourcePriority_t::MEDIUM; break;
	case resourceType_t::ASSET_IMAGE:	return resourcePriority_t::LOWEST; break;
	case resourceType_t::FRAMEBUFFER:	return resourcePriority_t::LOWEST; break;
	case resourceType_t::BINDSET:		return resourcePriority_t::LOWEST; break;
	case resourceType_t::IMAGE_SAMPLER:	return resourcePriority_t::LOWEST; break;
	default:							return resourcePriority_t::LOWEST; break;
	}
}


void RenderResource::Create( const resourceType_t type, const resourceLifeTime_t lifetime )
{
	m_type = type;
	m_lifetime = lifetime;
	m_priority = GetPriorityForType( type );

	m_resourceMemoryRegion = memoryRegion_t::UNKNOWN;
	m_resourceByteCount = 0;

	switch ( m_lifetime )
	{
	case resourceLifeTime_t::TASK:		InsertSorted( m_taskDependentResources,  this ); break;
	case resourceLifeTime_t::FRAME:		InsertSorted( m_frameDependentResources, this ); break;
	case resourceLifeTime_t::RESIZE:	InsertSorted( m_viewDependentResources,  this ); break;
	case resourceLifeTime_t::REBOOT:	InsertSorted( m_appDependentResources,   this ); break;
	}
}