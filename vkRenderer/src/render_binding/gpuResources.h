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

#pragma once
#include "../globals/common.h"
#include "allocator.h"

struct GpuBuffer
{
	AllocationVk alloc;

	void Reset() {
		offset = 0;
	}

	uint64_t GetSize() const {
		return offset;
	}

	uint64_t GetMaxSize() const {
		return alloc.GetSize();
	}

	void Allocate( const uint64_t size ) {
		offset += size;
	}

	VkBuffer& GetVkObject() {
		return buffer;
	}

	void CopyData( void* data, const size_t sizeInBytes )
	{
		assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
		void* mappedData = alloc.GetPtr();
		if ( mappedData != nullptr )
		{
			memcpy( (uint8_t*)mappedData + offset, data, sizeInBytes );
			offset += static_cast<uint32_t>( ( sizeInBytes + ( alloc.GetAlignment() - 1 ) ) & ~( alloc.GetAlignment() - 1 ) );
		}
	}

private:
	uint64_t	offset;
	VkBuffer	buffer;
};