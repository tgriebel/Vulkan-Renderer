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

#include <SysCore/timer.h>

#include "../globals/common.h"
#include "../globals/renderConstants.h"
#include "../globals/renderview.h"
#include "../globals/postEffect.h"

#include "../render_state/cmdContext.h"
#include "../render_binding/bufferObjects.h"
#include "../render_core/RenderTask.h"

class Window;
class SwapChain;
class Scene;

using renderPassMap_t = std::unordered_map<uint64_t, VkRenderPass>;
using pipelineMap_t = std::unordered_map<uint64_t, pipelineObject_t>;
using bindSetMap_t = std::unordered_map<uint64_t, ShaderBindSet>;

#if defined( USE_IMGUI )
extern imguiControls_t g_imguiControls;
#endif

extern Window						g_window;
extern SwapChain					g_swapChain;

struct renderConfig_t
{
	imageSamples_t	mainColorSubSamples;
	bool			present;
};


struct ComputeState
{
	int32_t				x;
	int32_t				y;
	int32_t				z;
	bool				updateDescriptorSets;
	ShaderBindParms*	parms;
};


class RenderContext
{
public:
	GpuBuffer			globalConstants;
	GpuBuffer			surfParms;
	GpuBuffer			materialBuffers;
	GpuBuffer			lightParms;
	GpuBuffer			particleBuffer;

	// TODO: move view-specific data
	GpuBuffer			viewParms;
	ImageView			depthImageView;
	ImageView			stencilImageView;
	GpuBufferView		surfParmPartitions[ MaxViews ]; // "View" is used in two ways here: view of data, and view of scene

	// Memory
	AllocatorMemory		localMemory;
	AllocatorMemory		frameBufferMemory;
	AllocatorMemory		sharedMemory;
};


class Renderer
{
public:
	friend class RenderTask;

	void								Init();
	void								Shutdown();
	void								Commit( const Scene* scene );
	void								Render();

	void								InitGPU();
	void								ShutdownGPU();
	void								Resize();

	void								AttachDebugMenu( const debugMenuFuncPtr funcPtr );

private:
	using committedLightsArray_t	= Array<lightBufferObject_t, MaxLights>;
	using materialBufferArray_t		= Array<materialBufferObject_t, MaxMaterials>;
	using bindParmArray_t			= Array<ShaderBindParms, DescriptorPoolMaxSets>;
	using surfUploadArray_t			= Array<surfaceUpload_t, MaxSurfaces* MaxViews>;

	static const uint32_t				ShadowMapWidth = 1024;
	static const uint32_t				ShadowMapHeight = 1024;
	static const uint32_t				OutlineStencilBit = 0x01;

	renderConfig_t						config;
	renderConstants_t					rc;
	RenderView							views[ MaxViews ];
	RenderView*							activeViews[ MaxViews ];
	RenderView*							renderViews[ Max3DViews ];
	RenderView*							shadowViews[ MaxShadowViews ];
	RenderView*							view2Ds[ Max2DViews ];
	ImageProcess						downScale;
	ImageProcess*						resolve = nullptr;
	uint32_t							viewCount;
	uint32_t							activeViewCount;

	RenderSchedule						schedule;

	// Timers
	Timer								frameTimer;
	uint32_t							m_frameNumber = 0;

	// Upload management
	std::set<hdl_t>						uploadTextures;
	std::set<hdl_t>						updateTextures;
	std::set<hdl_t>						uploadMaterials;

	uint32_t							imageFreeSlot = 0;
	uint32_t							vbBufElements = 0;
	uint32_t							ibBufElements = 0;
	
	// Render context
	RenderContext						renderContext;
	GfxContext							gfxContext;
	ComputeContext						computeContext;
	UploadContext						uploadContext;
	ComputeState						particleState;
	GpuSemaphore						uploadFinishedSemaphore;

	// Shader resources
	GpuBuffer							geoStagingBuffer;
	GpuBuffer							textureStagingBuffer;
	GpuBuffer							vb;	// move
	GpuBuffer							ib;
	ImageArray							gpuImages2D;
	ImageArray							gpuImagesCube;
	materialBufferArray_t				materialBuffer;
	committedLightsArray_t				committedLights;
	surfUploadArray_t					surfUploads;

	Image								shadowMapImage[ MaxShadowViews ];
	Image								mainColorImage;
	Image								mainColorResolvedImage;
	Image								tempColorImage;
	Image								mainColorDownsampled;
	Image								depthStencilImage;
	Image								depthStencilResolvedImage;

	ImageView							depthResolvedImageView;
	ImageView							stencilResolvedImageView;

	FrameBuffer							shadowMap[ MaxShadowMaps ];
	FrameBuffer							mainColor;
	FrameBuffer							mainColorResolved;
	FrameBuffer							tempColor;

	uint32_t							shadowCount = 0;

	// Shader binding
	bindParmArray_t						bindParmsList;
	bindSetMap_t						bindSets;

	// Init/Shutdown
	void								InitApi();
	void								InitShaderResources();
	void								InitConfig();
	void								InitImGui( RenderView& view );
	void								ShutdownImGui();
	void								ShutdownShaderResources();
	void								Cleanup();
	void								RecreateSwapChain();

	// API Resource Functions
	void								CreateImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage );
	ShaderBindParms*					RegisterBindParm( const ShaderBindSet* set );
	void								AllocRegisteredBindParms();
	void								FreeRegisteredBindParms();
	void								RefreshRegisteredBindParms();
	void								CreateSyncObjects();
	void								CreateCodeTextures();
	void								CreateBuffers();
	void								CreateFramebuffers();
	void								DestroyFramebuffers();

	// Draw Frame
	void								CommitModel( RenderView& view, const Entity& ent );
	void								MergeSurfaces( RenderView& view );
	void								FlushGPU();
	void								WaitForEndFrame();
	void								SubmitFrame();

	void								CommitViews( const Scene* scene );
	void								CommitLight( const light_t& light );

	// Update/Upload
	void								BeginUploadCommands( UploadContext& uploadContext );
	void								EndUploadCommands( UploadContext& uploadContext );
	void								CopyGpuBuffer( GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );

	void								UploadAssets();
	void								UpdateTextureData();
	void								UploadTextures();
	void								UpdateGpuMaterials();
	void								UploadModelsToGPU();
	void								UpdateBindSets();
	void								UpdateBuffers();
	void								UpdateFrameDescSet();
	void								BuildPipelines();
};