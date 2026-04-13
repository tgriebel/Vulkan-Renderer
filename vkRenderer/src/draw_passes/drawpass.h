#pragma once

#include "../globals/common.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../render_binding/imageArray.h"
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
	virtual void Init( RenderContext* renderContext, FrameBuffer* fb ) = 0;

	virtual void FrameBegin( const ResourceContext* resources ) = 0;
	virtual void FrameEnd() = 0;

	inline void Resize()
	{
		SetFrameBuffer( m_fb );
	}

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

	ImageArray					codeImages;
	ImageArray					codeCubeImages;
	ShaderBindParms*			parms;
};