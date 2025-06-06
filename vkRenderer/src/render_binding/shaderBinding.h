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
#include "../render_core/renderResource.h"

class GpuBuffer;
class GpuImage;

enum class bindType_t
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


enum class bindSemantic_t
{
	UNKNOWN,
	BUFFER,
	IMAGE,
	IMAGE_ARRAY,
};


enum bindStateFlag_t
{
	BIND_STATE_VS = ( 1 << 0 ),
	BIND_STATE_PS = ( 1 << 1 ),
	BIND_STATE_CS = ( 1 << 2 ),
	BIND_STATE_ALL = ( 1 << 3 ) - 1
};


static inline bindSemantic_t GetBindSemantic( const bindType_t bindType )
{
	bindSemantic_t bindSemantic = bindSemantic_t::UNKNOWN;
	switch ( bindType )
	{
		case bindType_t::CONSTANT_BUFFER:
		case bindType_t::READ_BUFFER:
		case bindType_t::WRITE_BUFFER:
		case bindType_t::READ_IMAGE_BUFFER:
		case bindType_t::WRITE_IMAGE_BUFFER:
			return bindSemantic_t::BUFFER;

		case bindType_t::IMAGE_2D:
		case bindType_t::IMAGE_3D:
		case bindType_t::IMAGE_CUBE:
			return bindSemantic_t::IMAGE;

		case bindType_t::IMAGE_2D_ARRAY:
		case bindType_t::IMAGE_CUBE_ARRAY:
			return bindSemantic_t::IMAGE_ARRAY;
	}
	return bindSemantic_t::UNKNOWN;
}


class ShaderBindSet;

class ShaderBinding
{
private:
	struct bindState_t
	{
	private:
		bindType_t		type;
		uint32_t		slot;
		uint32_t		maxDescriptorCount;
		bindStateFlag_t	flags;

		friend ShaderBinding;
	};
	bindState_t		m_state;
	uint32_t		m_hash;
	const char*		m_name;

	inline void SetSlot( const uint32_t slot )
	{
		m_state.slot = slot;
	}
public:

	ShaderBinding() {}

	ShaderBinding( const char* name, const bindType_t type, const uint32_t count, const bindStateFlag_t flags );

	uint32_t		GetSlot() const;
	bindType_t		GetType() const;
	bool			IsArrayType() const;
	uint32_t		GetMaxDescriptorCount() const;
	bindStateFlag_t	GetBindFlags() const;
	uint32_t		GetHash() const;

	friend class ShaderBindSet;
};


class ShaderAttachment
{
private:
	union attach_t
	{
		const GpuBuffer*	buffer;
		const Image*		image;
		const ImageArray*	imageArray;
		const void*			ptr;
	} u;
	bindSemantic_t semantic;
public:


	ShaderAttachment()
	{
		u.ptr = nullptr;
		semantic = bindSemantic_t::UNKNOWN;
	}

	ShaderAttachment( const GpuBuffer* buffer )
	{
		u.buffer = buffer;
		semantic = bindSemantic_t::BUFFER;
	}

	ShaderAttachment( const Image* image )
	{
		u.image = image;
		semantic = bindSemantic_t::IMAGE;
	}

	ShaderAttachment( const ImageArray* imageArray )
	{
		u.imageArray = imageArray;
		semantic = bindSemantic_t::IMAGE_ARRAY;
	}

	inline bool operator==( const ShaderAttachment& rhs ) const
	{
		return ( u.ptr == rhs.u.ptr );
	}

	inline bool operator!=( const ShaderAttachment& rhs ) const
	{
		return ( u.ptr != rhs.u.ptr );
	}

	inline bindSemantic_t GetSemantic() const
	{
		return semantic;
	}

	inline const GpuBuffer* GetBuffer() const
	{
		return ( semantic == bindSemantic_t::BUFFER ) ? u.buffer : nullptr;
	}

	inline const Image* GetImage() const
	{
		return ( semantic == bindSemantic_t::IMAGE ) ? u.image : nullptr;
	}

	inline const ImageArray* GetImageArray() const
	{
		return ( semantic == bindSemantic_t::IMAGE_ARRAY ) ? u.imageArray : nullptr;
	}
};


class ShaderBindSet : public RenderResource
{
private:
	std::unordered_map<uint32_t, ShaderBinding> m_bindMap;
	uint32_t									m_hash;
	bool										m_valid;

#ifdef USE_VULKAN
	VkDescriptorSetLayout						vk_layout;
#endif
public:

	ShaderBindSet()
	{}

	void Create( const char* name, const ShaderBinding bindings[], const uint32_t bindCount );
	void Destroy();

#ifdef USE_VULKAN
	inline VkDescriptorSetLayout GetVkObject() const
	{
		return vk_layout;
	}
#endif

	const uint32_t			Count() const;
	const uint32_t			GetHash() const;
	const ShaderBinding*	GetBinding( const uint32_t id ) const;
	bool					HasBinding( const uint32_t id ) const;
	bool					HasBinding( const ShaderBinding& binding ) const;
};


class ShaderBindParms
{
private:
	const ShaderBindSet*							bindSet;
	std::unordered_map<uint32_t, ShaderAttachment>	attachments[ MaxFrameStates ];
	std::unordered_map<uint32_t, bool>				dirty[ MaxFrameStates ];

#ifdef USE_VULKAN
	VkDescriptorSet									vk_descriptorSets[ MaxFrameStates ];
#endif

public:

#ifdef USE_VULKAN
	ShaderBindParms()
	{
		InitApiObjects();
	}

	ShaderBindParms::ShaderBindParms( const ShaderBindSet* set )
	{
		bindSet = set;
		for ( uint32_t i = 0; i < MaxFrameStates; ++i ) {
			attachments[ i ].reserve( bindSet->Count() );
		}

		InitApiObjects();
	}

	VkDescriptorSet				GetVkObject() const;
	void						SetVkObject( const VkDescriptorSet descSet[ MaxFrameStates ] );
	void						InitApiObjects();
#endif

	inline const ShaderBindSet*	GetSet() const
	{
		return bindSet;
	}

	bool						IsValid() const;
	bool						AttachmentChanged( const ShaderBinding& binding ) const;
	void						Clear();
	void						Bind( const ShaderBinding& binding, const ShaderAttachment attachment );
	void						Unbind( const ShaderBinding& binding );
	const ShaderAttachment*		GetAttachment( const ShaderBinding& binding ) const;
	const ShaderAttachment*		GetAttachment( const uint32_t id ) const;
};