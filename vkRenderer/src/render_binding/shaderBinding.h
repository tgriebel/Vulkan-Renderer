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
#include <cstdint>
#include <vector>

class GpuBuffer;

enum bindType_t
{
	CONSTANT_BUFFER,
	IMAGE_2D,
	IMAGE_3D,
	IMAGE_CUBE,
	READ_BUFFER,
	WRITE_BUFFER,
	READ_IMAGE_BUFFER,
	WRITE_IMAGE_BUFFER,
};


enum bindStateFlag_t
{
	BIND_STATE_VS = ( 1 << 0 ),
	BIND_STATE_PS = ( 1 << 1 ),
	BIND_STATE_CS = ( 1 << 2 ),
	BIND_STATE_ALL = ( 1 << 3 ) - 1
};


class ShaderBinding
{
private:
	bindType_t		type;
	uint32_t		slot;
	uint32_t		descriptorCount;
	bindStateFlag_t	flags;
	uint32_t		hash;
public:

	ShaderBinding() {}

	ShaderBinding( const uint32_t slot, const bindType_t type, const uint32_t descriptorCount, const bindStateFlag_t flags );

	uint32_t		GetSlot() const;
	bindType_t		GetType() const;
	uint32_t		GetDescriptorCount() const;
	bindStateFlag_t	GetBindFlags() const;
	uint64_t		GetHash() const;
};


class ShaderBindSet
{
private:
	std::vector<const ShaderBinding*>	bindSlots;
	std::vector<const GpuBuffer*>		attachments;

	void	AddBind( const ShaderBinding* binding );
public:

	ShaderBindSet()
	{}

	ShaderBindSet( const ShaderBinding* bindings[], const uint32_t bindCount );

	const uint32_t			GetBindCount();
	const ShaderBinding*	GetBinding( uint32_t slot );
};