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