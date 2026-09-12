#pragma once

#include "resourceContext.h"

uint32_t ResourceContext::NextRenderView( GpuBufferView& renderViewBuffer )
{
	const uint32_t subBufferId = renderViewCount;

	renderViewBuffer = viewParms.GetBufferView( subBufferId, 1 );
	++renderViewCount;
	return subBufferId;
}