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

class DrawPass
{
private:
	viewport_t			viewport;
	FrameBuffer*		fb;

protected:
	const char*			name;
	drawPass_t			passId;
	imageSamples_t		sampleRate;


	void SetFrameBuffer( FrameBuffer* frameBuffer )
	{
		sampleRate = frameBuffer->SampleCount();
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = frameBuffer->GetWidth();
		viewport.height = frameBuffer->GetHeight();
		fb = frameBuffer;
	}

public:
	virtual void Init( FrameBuffer* fb ) = 0;

	inline drawPass_t Type() const
	{
		return passId;
	}

	inline const char* Name() const
	{
		return name;
	}

	inline gfxStateBits_t StateBits() const
	{
		return stateBits;
	}

	inline imageSamples_t SampleRate() const
	{
		return sampleRate;
	}

	inline void SetViewport( const uint32_t x, const uint32_t y, const uint32_t width, const uint32_t height )
	{
		viewport.x = x;
		viewport.y = y;
		viewport.width = width;
		viewport.height = height;
	}

	inline const viewport_t& GetViewport() const
	{
		return viewport;
	}

	inline const FrameBuffer* GetFrameBuffer() const
	{
		return fb;
	}

	inline FrameBuffer* GetFrameBuffer()
	{
		return fb;
	}

	gfxStateBits_t				stateBits;

	Array<Image*, 100>			codeImages;
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
};


class DepthPass : public DrawPass
{
public:
	DepthPass( FrameBuffer* fb )
	{
		Init(fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class OpaquePass : public DrawPass
{
public:
	OpaquePass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class TransPass : public DrawPass
{
public:
	TransPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class EmissivePass : public DrawPass
{
public:
	EmissivePass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class PostPass : public DrawPass
{
public:
	PostPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};



class TerrainPass : public DrawPass
{
public:
	TerrainPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class SkyboxPass : public DrawPass
{
public:
	SkyboxPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class WireframePass : public DrawPass
{
public:
	WireframePass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class Debug2dPass : public DrawPass
{
public:
	Debug2dPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};


class Debug3dPass : public DrawPass
{
public:
	Debug3dPass( FrameBuffer* fb )
	{
		Init( fb );
	}

	virtual void Init( FrameBuffer* fb );
};