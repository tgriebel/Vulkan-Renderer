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
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../render_state/frameBuffer.h"

class ResourceContext;

class DrawPass
{
private:
	viewport_t			m_viewport;
	FrameBuffer*		m_fb;

protected:
	const char*			m_name;
	drawPass_t			m_passId;
	imageSamples_t		m_sampleRate;
	gfxStateBits_t		m_stateBits;


	void SetFrameBuffer( FrameBuffer* frameBuffer )
	{
		m_sampleRate = frameBuffer->SampleCount();
		m_viewport.x = 0;
		m_viewport.y = 0;
		m_viewport.width = frameBuffer->GetWidth();
		m_viewport.height = frameBuffer->GetHeight();
		m_fb = frameBuffer;
	}

public:
	virtual void Init( FrameBuffer* fb ) = 0;

	virtual void FrameBegin( const ResourceContext* resources ) = 0;
	virtual void FrameEnd() = 0;

	inline drawPass_t Type() const
	{
		return m_passId;
	}

	inline const char* Name() const
	{
		return m_name;
	}

	inline gfxStateBits_t StateBits() const
	{
		return m_stateBits;
	}

	inline imageSamples_t SampleRate() const
	{
		return m_sampleRate;
	}

	inline void SetViewport( const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height )
	{
		m_viewport.x = x;
		m_viewport.y = y;
		m_viewport.width = width;
		m_viewport.height = height;
	}

	inline const viewport_t& GetViewport() const
	{
		return m_viewport;
	}

	inline const FrameBuffer* GetFrameBuffer() const
	{
		return m_fb;
	}

	inline FrameBuffer* GetFrameBuffer()
	{
		return m_fb;
	}

	void InsertResourceBarriers( CommandContext& cmdContext );

	Array<const Image*, 100>	codeImages;
	ShaderBindParms*			parms;
};


class ShadowPass : public DrawPass
{
public:
	ShadowPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class DepthPass : public DrawPass
{
public:
	DepthPass( FrameBuffer* fb )
	{
		Init(fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class OpaquePass : public DrawPass
{
public:
	OpaquePass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class TransPass : public DrawPass
{
public:
	TransPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class EmissivePass : public DrawPass
{
public:
	EmissivePass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class PostPass : public DrawPass
{
public:
	PostPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};



class TerrainPass : public DrawPass
{
public:
	TerrainPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class SkyboxPass : public DrawPass
{
public:
	SkyboxPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class WireframePass : public DrawPass
{
public:
	WireframePass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class Debug2dPass : public DrawPass
{
public:
	Debug2dPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};


class Debug3dPass : public DrawPass
{
public:
	Debug3dPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};