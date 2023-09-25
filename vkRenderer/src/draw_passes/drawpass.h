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
public:
	virtual void Init() = 0;
	drawPass_t GetType() const
	{
		return passId;
	}

	const char* GetName() const
	{
		return name;
	}

	const char*					name;

	drawPass_t					passId;
	gfxStateBits_t				stateBits;
	imageSamples_t				sampleRate;
	viewport_t					viewport;

	Array<Image*, 100>			codeImages;
	ShaderBindParms*			parms;
	FrameBuffer*				fb;

	bool						updateDescriptorSets;
};


class ShadowPass : public DrawPass
{
public:
	ShadowPass()
	{
		Init();
	}

	virtual void Init();
};


class DepthPass : public DrawPass
{
public:
	DepthPass()
	{
		Init();
	}

	virtual void Init();
};


class OpaquePass : public DrawPass
{
public:
	OpaquePass()
	{
		Init();
	}

	virtual void Init();
};


class TransPass : public DrawPass
{
public:
	TransPass()
	{
		Init();
	}

	virtual void Init();
};


class EmissivePass : public DrawPass
{
public:
	EmissivePass()
	{
		Init();
	}

	virtual void Init();
};


class PostPass : public DrawPass
{
public:
	PostPass()
	{
		Init();
	}

	virtual void Init();
};



class TerrainPass : public DrawPass
{
public:
	TerrainPass()
	{
		Init();
	}

	virtual void Init();
};


class SkyboxPass : public DrawPass
{
public:
	SkyboxPass()
	{
		Init();
	}

	virtual void Init();
};


class WireframePass : public DrawPass
{
public:
	WireframePass()
	{
		Init();
	}

	virtual void Init();
};


class Debug2dPass : public DrawPass
{
public:
	Debug2dPass()
	{
		Init();
	}

	virtual void Init();
};


class Debug3dPass : public DrawPass
{
public:
	Debug3dPass()
	{
		Init();
	}

	virtual void Init();
};