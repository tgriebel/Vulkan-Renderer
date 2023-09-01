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
#include "../render_core/gpuImage.h"

ShaderBinding::ShaderBinding( const char* name, const bindType_t type, const uint32_t descriptorCount, const bindStateFlag_t flags )
{
	m_state.slot = ~0x00;
	m_state.type = type;
	m_state.maxDescriptorCount = descriptorCount;
	m_state.flags = flags;

	const uint32_t bufSize = 128;

	const uint32_t sizeBytes = static_cast<uint32_t>( strlen( name ) );
	assert( sizeBytes <= bufSize );

	uint8_t hashBytes[ bufSize ];
	memcpy( hashBytes, &m_state, sizeBytes );

	m_hash = Hash( reinterpret_cast<const uint8_t*>( name ), sizeBytes );
	m_name = name;
}


uint32_t ShaderBinding::GetSlot() const
{
	return m_state.slot;
}


bindType_t ShaderBinding::GetType() const
{
	return m_state.type;
}


bool ShaderBinding::IsArrayType() const
{
	return ( GetBindSemantic( m_state.type ) == bindSemantic_t::IMAGE_ARRAY );
}


uint32_t ShaderBinding::GetMaxDescriptorCount() const
{
	return m_state.maxDescriptorCount;
}


bindStateFlag_t ShaderBinding::GetBindFlags() const
{
	return m_state.flags;
}


uint32_t ShaderBinding::GetHash() const
{
	return m_hash;
}


void ShaderBindSet::Create( const ShaderBinding bindings[], const uint32_t bindCount )
{
	if ( bindCount == 0 )
	{
		assert( 0 );
		return;
	}

	// Build bind set
	{
		m_bindMap.reserve( bindCount );

		std::vector<uint32_t> hashes;
		hashes.resize( bindCount );

		for ( uint32_t i = 0; i < bindCount; ++i )
		{
			ShaderBinding& binding = m_bindMap[ bindings[ i ].GetHash() ];
			binding = bindings[ i ];
			binding.SetSlot( i );
			hashes[ i ] = binding.GetHash();
		}

		m_hash = Hash( reinterpret_cast<const uint8_t*>( hashes.data() ), bindCount * sizeof( hashes[ 0 ] ) );
		m_valid = false;
	}

	// Create API object
#ifdef USE_VULKAN
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
	layoutBindings.resize( bindCount );

	for ( uint32_t i = 0; i < bindCount; ++i )
	{
		const ShaderBinding* binding = GetBinding( i );

		layoutBindings[ i ] = {};
		layoutBindings[ i ].binding = binding->GetSlot();
		layoutBindings[ i ].descriptorCount = binding->GetMaxDescriptorCount();
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

	m_valid = true;
}


void ShaderBindSet::Destroy()
{
	if( m_valid == false ) {
		return;
	}
#ifdef USE_VULKAN
	vkDestroyDescriptorSetLayout( context.device, vk_layout, nullptr );
	vk_layout = VK_NULL_HANDLE;
#endif
}


const uint32_t ShaderBindSet::Count() const
{
	return static_cast<uint32_t>( m_bindMap.size() );
}


const uint32_t ShaderBindSet::GetHash() const
{
	return m_hash;
}


const ShaderBinding* ShaderBindSet::GetBinding( const uint32_t id ) const
{
	auto it = m_bindMap.begin();
	std::advance( it, id );

	if ( it != m_bindMap.end() ) {
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
	auto it = m_bindMap.find( hash );
	if ( it != m_bindMap.end() ) {
		return true;
	}
	return false;
}


#ifdef USE_VULKAN
VkDescriptorSet ShaderBindParms::GetVkObject() const
{
	return vk_descriptorSets[ context.bufferId ];
}


void ShaderBindParms::SetVkObject( const VkDescriptorSet descSet[ MaxFrameStates ] )
{
	for ( uint32_t frameIx = 0; frameIx < MaxFrameStates; ++frameIx )
	{
		assert( descSet[ frameIx ] != VK_NULL_HANDLE );
		vk_descriptorSets[ frameIx ] = descSet[ frameIx ];
	}
}


void ShaderBindParms::InitApiObjects()
{
	for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
		vk_descriptorSets[ i ] = VK_NULL_HANDLE;
	}
}
#endif


bool ShaderBindParms::IsValid() const
{
	return ( static_cast<uint32_t>( attachments[ context.bufferId ].size() ) == bindSet->Count() );
}


bool ShaderBindParms::AttachmentChanged( const ShaderBinding& binding ) const
{
	const uint32_t hash = binding.GetHash();
	auto it = dirty[ context.bufferId ].find( hash );
	if( it != dirty[ context.bufferId ].end() ) {
		return it->second;
	}
	return false;
}


void ShaderBindParms::Bind( const ShaderBinding& binding, const ShaderAttachment attachment )
{
	if( bindSet->HasBinding( binding ) )
	{
		if( binding.IsArrayType() ) {
			assert( attachment.GetImageArray()->Count() <= binding.GetMaxDescriptorCount() );
		}
		assert( GetBindSemantic( binding.GetType() ) == attachment.GetSemantic() );

		const uint32_t hash = binding.GetHash();
		dirty[ context.bufferId ][ hash ] = ( attachments[ context.bufferId ][ hash ] != attachment );
		attachments[ context.bufferId ][ hash ] = attachment;
	} else {
		assert( 0 );
	}
}


const ShaderAttachment* ShaderBindParms::GetAttachment( const ShaderBinding& binding ) const
{
	auto it = attachments[ context.bufferId ].find( binding.GetHash() );
	if ( it != attachments[ context.bufferId ].end() ) {
		return &it->second;
	}
	return nullptr;
}


const ShaderAttachment* ShaderBindParms::GetAttachment( const uint32_t id ) const
{
	assert(0); // UNTESTED
	auto it = attachments[ context.bufferId ].begin();
	std::advance( it, id );

	if ( it != attachments[ context.bufferId ].end() ) {
		return &it->second;
	}
	return nullptr;
}