#include "shaderBinding.h"
#include "bindings.h"
#include "gpuResources.h"

ShaderBinding::ShaderBinding( const uint32_t slot, const bindType_t type, const uint32_t descriptorCount, const bindStateFlag_t flags )
	:	slot( slot ),
		type( type ),
		descriptorCount( descriptorCount ),
		flags( flags )
{
	const uint32_t sizeBytes = offsetof( ShaderBinding, hash );
	assert( sizeBytes <= 128 );

	uint8_t hashBytes[ 128 ];
	memcpy( hashBytes, this, sizeBytes );
	hash = Hash( hashBytes, COUNTARRAY( hashBytes ) );
}


uint32_t ShaderBinding::GetSlot() const
{
	return slot;
}


bindType_t ShaderBinding::GetType() const
{
	return type;
}


uint32_t ShaderBinding::GetDescriptorCount() const
{
	return descriptorCount;
}


bindStateFlag_t ShaderBinding::GetBindFlags() const
{
	return flags;
}


uint64_t ShaderBinding::GetHash() const
{
	return hash;
}


ShaderBindSet::ShaderBindSet( const ShaderBinding* bindings[], const uint32_t bindCount )
{
	bindSlots.resize( bindCount );

	for ( uint32_t i = 0; i < bindCount; ++i ) {
		AddBind( bindings[ i ] );
	}
}


void ShaderBindSet::AddBind( const ShaderBinding* binding )
{
	bindSlots[ binding->GetSlot() ] = binding;
}


const uint32_t ShaderBindSet::GetBindCount()
{
	return static_cast<uint32_t>( bindSlots.size() );
}


const ShaderBinding* ShaderBindSet::GetBinding( uint32_t slot )
{
	return bindSlots[ slot ];
}