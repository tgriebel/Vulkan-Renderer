#pragma once

#include <SysCore/timer.h>

#include "../globals/common.h"
#include "../globals/renderConstants.h"

#include "../render_state/cmdContext.h"
#include "../render_resources/imageSampler.h"
#include "../render_core/renderview.h"
#include "../render_core/renderResource.h"

#include "../render_tasks/RenderTask.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

#include "renderContext.h"
#include "resourceContext.h"
#include "renderUploader.h"

class Window;
class SwapChain;
class Scene;

#ifdef USE_VULKAN
using renderPassMap_t = std::unordered_map<uint64_t, VkRenderPass>;
#endif
using pipelineMap_t = std::unordered_map<uint64_t, pipelineObject_t>;

#define USE_OPENGL_CONVENTIONS 1

extern Window						g_window;
extern SwapChain					g_swapChain;

extern renderConstants_t	rc;

struct renderConfig_t
{
	imageSamples_t	mainColorSubSamples;
	const char*		cubemapName;
	bool			present;
	bool			useCubeViews;
	bool			cubeDownsample;
	bool			writeCubeViews;
	bool			computeEnvMap;
	bool			computeDiffuseIbl;
	bool			computeSpecularIBL;
	bool			downsampleScene;
	bool			bloom;
	bool			autoExposure;
	bool			screenshot;
	bool			gaussianBlur;
	bool			shadows;
	bool			computeBrdfLut;
};


struct ComputeState
{
	ShaderBindParms*	parms;
	int32_t				x;
	int32_t				y;
	int32_t				z;
};


class Renderer
{
public:
	friend class RenderTask;

	void								Init( const renderConfig_t& cfg );
	void								Shutdown();
	void								Commit( const Scene* scene );
	void								Render();

	void								InitGPU();
	void								ShutdownGPU();
	void								Resize();

	inline void							SetSchedule( const RenderSchedule* schedule )
	{

	}

	// Debug
	uint32_t							OutputImageCount();
	const Image*						FindOutputImage( const uint32_t id );

private:
	using committedLightsArray_t	= Array<gpuLight_t, MaxLights>;

	static const uint32_t				ShadowMapWidth = 2048;
	static const uint32_t				ShadowMapHeight = 2048;
	static const uint32_t				OutlineStencilBit = 0x01;

	renderConfig_t						config;
	RenderView							views[ MaxViews ];
	RenderView*							activeViews[ MaxViews ];
	RenderView*							renderViews[ Max3DViews ];
	RenderView*							shadowViews[ MaxShadowViews ];
	RenderView*							view2Ds[ Max2DViews ];
	uint32_t							viewCount;
	uint32_t							activeViewCount;

	RenderSchedule						schedule;

	// Timers
	SysCore::Timer						frameTimer;

	// Upload management
	RenderUploader						uploader;
	std::set<hdl_t>						uploadMaterials;
	
	// Render context
	RenderContext						renderContext;
	GfxCmdList							gfxContext;
	ComputeCmdList						computeContext;
	ComputeState						particleState;

	// Shader resources
	ResourceContext						resources;
	committedLightsArray_t				committedLights;

	uint32_t							shadowCount = 0;

	// Init/Shutdown
	void								InitApi( const renderConfig_t& cfg );
	void								InitShaderResources();
	void								AssignBindSetsToGpuProgs();
	void								InitConfig( const renderConfig_t& cfg );
	void								InitImGui( const FrameBuffer* fb );
	void								ShutdownImGui();
	void								ShutdownShaderResources();
	void								Destroy();
	void								RecreateSwapChain();

	// API Resource Functions
	void								CreateSyncObjects();
	void								CreateFramebuffers();

	// Draw Frame
	void								CommitModel( RenderView& view, const Entity& ent );
	void								WaitForEndFrame();
	void								SubmitFrame();

	void								CommitViews( const Scene* scene );
	void								CommitLight( const light_t& light );

	// Update/Upload
	void								UploadAssets();
	void								UpdateBindSets();
	void								UpdateBuffers();
	void								BuildPipelines();
};
