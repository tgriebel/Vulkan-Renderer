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
#include <unordered_map>
#include "../globals/common.h"

class GpuBuffer;
class GpuImage;

enum bindType_t
{
	CONSTANT_BUFFER,
	IMAGE_2D,
	IMAGE_2D_ARRAY,
	IMAGE_3D,
	IMAGE_CUBE,
	IMAGE_CUBE_ARRAY,
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

class ShaderBindSet;

class ShaderBinding
{
private:
	struct bindState_t
	{
	private:
		bindType_t		type;
		uint32_t		slot;
		uint32_t		descriptorCount;
		bindStateFlag_t	flags;

		friend ShaderBinding;
	} state;
	uint32_t		hash;

	inline void SetSlot( const uint32_t slot )
	{
		state.slot = slot;
	}
public:

	ShaderBinding() {}

	ShaderBinding( const char* name, const bindType_t type, const uint32_t count, const bindStateFlag_t flags );

	uint32_t		GetSlot() const;
	bindType_t		GetType() const;
	uint32_t		GetDescriptorCount() const;
	bindStateFlag_t	GetBindFlags() const;
	uint32_t		GetHash() const;

	friend class ShaderBindSet;
};


class ShaderAttachment
{
public:
	enum class type_t
	{
		BUFFER,
		IMAGE,
		IMAGE_ARRAY,
	};
private:
	union attach_t
	{
		const GpuBuffer*		buffer;
		const GpuImage*			image;
		const GpuImage**		imageArray;
	} u;
	type_t type;
public:


	ShaderAttachment()
	{}

	ShaderAttachment( const GpuBuffer* buffer )
	{
		u.buffer = buffer;
		type = type_t::BUFFER;
	}

	ShaderAttachment( const GpuImage* image )
	{
		u.image = image;
		type = type_t::IMAGE;
	}

	ShaderAttachment( const GpuImage* imageArray[] )
	{
		u.imageArray = imageArray;
		type = type_t::IMAGE_ARRAY;
	}

	inline type_t GetType() const
	{
		return type;
	}

	inline const GpuBuffer* GetBuffer() const
	{
		return ( type == type_t::BUFFER ) ? u.buffer : nullptr;
	}

	inline const GpuImage* GetImage() const
	{
		return ( type == type_t::IMAGE ) ? u.image : nullptr;
	}

	inline const GpuImage** GetImageArray() const
	{
		return ( type == type_t::IMAGE_ARRAY ) ? u.imageArray : nullptr;
	}
};


class ShaderBindSet
{
private:
	std::unordered_map<uint32_t, ShaderBinding> bindMap;
	bool										valid;

#ifdef USE_VULKAN
	VkDescriptorSetLayout						vk_layout;
#endif
public:

	ShaderBindSet()
	{}



	ShaderBindSet( const ShaderBinding bindings[], const uint32_t bindCount );

	void Create();
	void Destroy();

#ifdef USE_VULKAN
	inline VkDescriptorSetLayout GetVkObject() const
	{
		return vk_layout;
	}
#endif

	const uint32_t			Count() const;
	const ShaderBinding*	GetBinding( const uint32_t id ) const;
	bool					HasBinding( const uint32_t id ) const;
	bool					HasBinding( const ShaderBinding& binding ) const;
};


class ShaderBindParms
{
private:
	const ShaderBindSet*							bindSet;
	std::unordered_map<uint32_t, ShaderAttachment>	attachments;

#ifdef USE_VULKAN
	VkDescriptorSet									vk_descriptorSet;
#endif

public:

#ifdef USE_VULKAN
	ShaderBindParms()
	{
		vk_descriptorSet = VK_NULL_HANDLE;
	}

	ShaderBindParms::ShaderBindParms( const ShaderBindSet* set )
	{
		bindSet = set;
		attachments.reserve( bindSet->Count() );

		InitApiObjects();
	}

	inline VkDescriptorSet GetVkObject()
	{
		return vk_descriptorSet;
	}

	inline void SetVkObject( const VkDescriptorSet descSet )
	{
		vk_descriptorSet = descSet;
	}

	void InitApiObjects()
	{
		vk_descriptorSet = VK_NULL_HANDLE;
	}

	inline bool IsValid()
	{
		return ( vk_descriptorSet != VK_NULL_HANDLE );
	}
#endif

	inline const ShaderBindSet* GetSet() const
	{
		return bindSet;
	}

	void Bind( const ShaderBinding& binding, const ShaderAttachment& attachment );
	const ShaderAttachment* GetAttachment( const ShaderBinding* binding ) const;
	const ShaderAttachment* GetAttachment( const uint32_t id ) const;
};