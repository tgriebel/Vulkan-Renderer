/*
* MIT License
*
* Copyright( c ) 2023 Thomas Griebel
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this softwareand associated documentation files( the "Software" ), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright noticeand this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "shaderBinding.h"
#include "bindings.h"
#include "gpuResources.h"
#include "../render_state/rhi.h"

ShaderBinding::ShaderBinding( const char* name, const bindType_t type, const uint32_t descriptorCount, const bindStateFlag_t flags )
{
	state.slot = ~0x00;
	state.type = type;
	state.descriptorCount = descriptorCount;
	state.flags = flags;

	const uint32_t bufSize = 128;

	const uint32_t sizeBytes = static_cast<uint32_t>( strlen( name ) );
	assert( sizeBytes <= bufSize );

	uint8_t hashBytes[ bufSize ];
	memcpy( hashBytes, &state, sizeBytes );
	hash = Hash( reinterpret_cast<const uint8_t*>( name ), sizeBytes );
}


uint32_t ShaderBinding::GetSlot() const
{
	return state.slot;
}


bindType_t ShaderBinding::GetType() const
{
	return state.type;
}


uint32_t ShaderBinding::GetDescriptorCount() const
{
	return state.descriptorCount;
}


bindStateFlag_t ShaderBinding::GetBindFlags() const
{
	return state.flags;
}


uint32_t ShaderBinding::GetHash() const
{
	return hash;
}


ShaderBindSet::ShaderBindSet( const ShaderBinding bindings[], const uint32_t bindCount )
{
	bindMap.reserve( bindCount );

	for ( uint32_t i = 0; i < bindCount; ++i )
	{
		ShaderBinding& binding = bindMap[ bindings[ i ].GetHash() ];
		binding = bindings[i];
		binding.SetSlot( i );
	}

	valid = false;
}


void ShaderBindSet::Create()
{
	const uint32_t bindingCount = GetBindCount();
	if ( bindingCount == 0 )
	{
		assert( 0 );
		return;
	}
	assert( valid == false );

#ifdef USE_VULKAN
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.resize( bindingCount );

	for ( uint32_t i = 0; i < bindingCount; ++i )
	{
		const ShaderBinding* binding = GetBinding( i );

		layoutBindings[ i ] = {};
		layoutBindings[ i ].binding = binding->GetSlot();
		layoutBindings[ i ].descriptorCount = binding->GetDescriptorCount();
		layoutBindings[ i ].descriptorType = vk_GetDescriptorType( binding->GetType() );
		layoutBindings[ i ].pImmutableSamplers = nullptr;
		layoutBindings[ i ].stageFlags = vk_GetStageFlags( binding->GetBindFlags() );
	}

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>( layoutBindings.size() );
	layoutInfo.pBindings = layoutBindings.data();

	if ( vkCreateDescriptorSetLayout( context.device, &layoutInfo, nullptr, &vk_layout ) != VK_SUCCESS ) {
		throw std::runtime_error( "CreateBindingLayout: Failed to create compute descriptor set layout!" );
	}
#else
	assert(0);
#endif

	valid = true;
}


void ShaderBindSet::Destroy()
{
	if( valid == false ) {
		return;
	}
#ifdef USE_VULKAN
	vkDestroyDescriptorSetLayout( context.device, vk_layout, nullptr );
#endif
}


const uint32_t ShaderBindSet::GetBindCount() const
{
	return static_cast<uint32_t>( bindMap.size() );
}


const ShaderBinding* ShaderBindSet::GetBinding( const uint32_t id ) const
{
	auto it = bindMap.begin();
	std::advance( it, id );

	if ( it != bindMap.end() ) {
		return &it->second;
	}
	return nullptr;
}


bool ShaderBindSet::HasBinding( const uint32_t id ) const
{
	return ( GetBinding( id ) != nullptr );
}


bool ShaderBindSet::HasBinding( const ShaderBinding& binding ) const
{
	const uint32_t hash = binding.GetHash();
	auto it = bindMap.find( hash );
	if ( it != bindMap.end() ) {
		return true;
	}
	return false;
}


void ShaderBindParms::Bind( const ShaderBinding& binding, const ShaderAttachment& attachment )
{
	if( bindSet->HasBinding( binding ) ) {
		attachments[ binding.GetHash() ] = attachment;
	}
}


const ShaderAttachment* ShaderBindParms::GetAttachment( const ShaderBinding& binding ) const
{
	auto it = attachments.find( binding.GetHash() );
	if ( it != attachments.end() ) {
		return &it->second;
	}
	return nullptr;
}