#pragma once

/*
* MIT License
*
* Copyright( c ) 2026 Thomas Griebel
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

#include <SysCore/bitArray.h>
#include "../render_core/gpuImage.h"
#include "../render_state/deviceContext.h"
#include "../asset_types/texture.h"

using BaseImageArray = Array<const Image*, MaxImageDescriptors>;

class ImageArray : public BaseImageArray
{
private:
	uint64_t		m_lastFrameUpdate[MaxImageDescriptors];
	RenderContext* m_context;

public:
	ImageArray()
	{
		m_context = nullptr;

		memset(m_lastFrameUpdate, 0, MaxImageDescriptors * sizeof(uint64_t));
	}

	const Image* operator[](uint32_t index) const
	{
		return BaseImageArray::operator[](index);
	}

	// This name makes the intent more explicit than [] and allows additional book-keeping easily
	void BindIndex(const uint32_t index, const Image* image);

	void SetRenderContext(RenderContext* context);

	[[nodiscard]]
	bool HasPossibleUpdates() const;
};
