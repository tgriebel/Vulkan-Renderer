#pragma once

#include "renderer.h"

// FIXME: Temp, remove once interface becomes clear
// Intentionally lazy pointers to arrays because this will be removed
struct RenderViewContext
{
	RenderView** activeViews;
	RenderView** renderViews;
	RenderView** shadowViews;
	RenderView** view2Ds;
};

void BuildSceneSchedule( const renderConfig_t& config, RenderContext* renderContext, ResourceContext* resourceContext, RenderViewContext* viewContext, RenderSchedule* schedule );
