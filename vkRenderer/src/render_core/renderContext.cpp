#include "renderer.h"
#include "../render_binding/bindings.h"

ShaderBindParms* RenderContext::RegisterBindParm( const ShaderBindSet* set )
{
	const uint32_t id = bindParmsList.Count();

	ShaderBindParms parms = ShaderBindParms( set, id );

	pendingIndices.Append( id );
	bindParmsList.Append( parms );

	return &bindParmsList[ id ];
}


ShaderBindParms* RenderContext::RegisterBindParm( const uint64_t setId )
{
	return RegisterBindParm( &bindSets[ setId ] );
}


ShaderBindParms* RenderContext::RegisterBindParm( const char* setName )
{
	return RegisterBindParm( &bindSets[ Hash( setName ) ] );
}


const ShaderBindSet* RenderContext::LookupBindSet( const uint64_t setId ) const
{
	auto it = bindSets.find( setId );
	if( it != bindSets.end() ) {
		return &it->second;
	}
	else {
		return nullptr;
	}
}


const ShaderBindSet* RenderContext::LookupBindSet( const char* name ) const
{
	return LookupBindSet( Hash( name ) );
}


void RenderContext::AllocRegisteredBindParms()
{
	//SCOPED_TIMER_PRINT( AllocRegisteredBindParms )

	const uint32_t pendingParmCount = pendingIndices.Count();
	if( pendingParmCount == 0 ) {
		return;
	}

	std::vector<VkDescriptorSetLayout> layouts;
	std::vector<VkDescriptorSet> descSets;
	std::vector<const char*> descNames;

	layouts.reserve( MaxFrameStates * pendingParmCount );
	descSets.reserve( MaxFrameStates * pendingParmCount );
	descNames.reserve( MaxFrameStates * pendingParmCount );

	for( uint32_t i = 0; i < pendingParmCount; ++i ) {
		const uint32_t bindIx = pendingIndices[ i ];
		ShaderBindParms& parms = bindParmsList[ bindIx ];
		const ShaderBindSet* set = parms.GetSet();

		for( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx ) {
			layouts.push_back( set->GetVkObject() );
			descSets.push_back( VK_NULL_HANDLE );
			descNames.push_back( set->GetName() );
		}
	}

	VkDescriptorSetAllocateInfo allocInfo { };
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = context.descriptorPool;
	allocInfo.descriptorSetCount = static_cast< uint32_t >( layouts.size() );
	allocInfo.pSetLayouts = layouts.data();

	VK_CHECK_RESULT( vkAllocateDescriptorSets( context.device, &allocInfo, descSets.data() ) );

	for( uint32_t i = 0; i < pendingParmCount; ++i ) {
		vk_SetObjectName( ( uint64_t )descSets[ i ], VK_OBJECT_TYPE_DESCRIPTOR_SET, vk_BuildObjectName( "DescriptorSet", descNames[ i ] ).c_str() );
	}

	for( uint32_t i = 0; i < pendingParmCount; ++i ) {
		const uint32_t bindIx = pendingIndices[ i ];
		ShaderBindParms& parms = bindParmsList[ bindIx ];
		parms.SetVkObject( &descSets[ MaxFrameStates * i ] );
	}
	pendingIndices.Reset();
}


void RenderContext::FreeRegisteredBindParms()
{
	std::vector<VkDescriptorSet> descSets;
	descSets.reserve( bindParmsList.Count() );

	for( uint32_t i = 0; i < bindParmsList.Count(); ++i ) {
		ShaderBindParms& parms = bindParmsList[ i ];
		descSets.push_back( parms.GetVkObject() );
	}

	vkFreeDescriptorSets( context.device, context.descriptorPool, static_cast< uint32_t >( descSets.size() ), descSets.data() );

	pendingIndices.Reset();
	bindParmsList.Reset();
}


void RenderContext::RefreshRegisteredBindParms()
{
	for( uint32_t i = 0; i < bindParmsList.Count(); ++i ) {
		bindParmsList[ i ].Clear();
	}
}
