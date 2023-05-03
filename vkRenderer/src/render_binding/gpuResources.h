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

class GpuBuffer
{
public:
	static uint64_t	GetPadding( const uint64_t size, const uint64_t alignment );

	void			SetPos( const uint32_t pos = 0 );
	uint64_t		GetSize() const;
	uint64_t		GetMaxSize() const;
	void			Allocate( const uint64_t size );
	VkBuffer&		VkObject();
	VkBuffer		GetVkObject() const;
	void			Create( const uint64_t size, VkBufferUsageFlags usage, AllocatorVkMemory& bufferMemory );
	void			Destroy();
	bool			VisibleToCpu() const;
	void			CopyData( void* data, const size_t sizeInBytes );

private:
	alloc_t<Allocator<VkDeviceMemory>> alloc;
	uint64_t	offset;
	VkBuffer	buffer;
};