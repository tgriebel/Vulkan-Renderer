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

extern renderConstants_t	rc;

// TODO: Figure out where to put this
void CreateImage( const char* name, const imageInfo_t& info, const gpuImageStateFlags_t flags, AllocatorMemory& memory, Image& outImage );

struct renderConfig_t
{
	imageSamples_t	mainColorSubSamples;
	bool			present;
};


struct ComputeState
{
	ShaderBindParms*	parms;
	int32_t				x;
	int32_t				y;
	int32_t				z;
};


// Renderer system and resources accessible to sub-systems
class RenderContext
{
private:
	using bindParmArray_t = Array<ShaderBindParms, DescriptorPoolMaxSets>;
	using pendingArray_t = Array<uint32_t, DescriptorPoolMaxSets>;

	// Shader binding
	bindParmArray_t			bindParmsList;
	pendingArray_t			pendingIndices;
	bindSetMap_t			bindSets;

public:
	ShaderBindParms*		globalParms;

	// Memory
	AllocatorMemory			localMemory;
	AllocatorMemory			frameBufferMemory;
	AllocatorMemory			sharedMemory;

	ShaderBindParms*		RegisterBindParm( const ShaderBindSet* set );
	ShaderBindParms*		RegisterBindParm( const uint64_t setId );
	ShaderBindParms*		RegisterBindParm( const char* setName );
	const ShaderBindSet*	LookupBindSet( const uint64_t setId ) const;
	const ShaderBindSet*	LookupBindSet( const char* name ) const;
	void					UpdateBindParms();
	void					AllocRegisteredBindParms();
	void					FreeRegisteredBindParms();
	void					RefreshRegisteredBindParms();

	friend class Renderer; // TODO: Need an interface for bind sets
};


// Bundle of all resources needed to represent geometry on the GPU
class GeometryContext
{
public:
	using surfUploadArray_t	= Array<surfaceUpload_t, MaxSurfaces * MaxViews>;

	GpuBuffer			stagingBuffer;
	GpuBuffer			vb;
	GpuBuffer			ib;
	surfUploadArray_t	surfUploads;

	uint32_t			vbBufElements = 0;
	uint32_t			ibBufElements = 0;
};


// Resources that are globally accessible for shader binding
class ResourceContext
{
public:
	GpuBuffer			globalConstants;
	GpuBuffer			surfParms;
	GpuBuffer			materialBuffers;
	GpuBuffer			lightParms;
	GpuBuffer			particleBuffer;

	// TODO: move view-specific data
	GpuBuffer			viewParms;
	Image				mainColorImage;
	Image				cubeFbColorImage;
	Image				cubeFbDepthImage;
	Image				diffuseIblImage;
	ImageView			cubeFbImageView;
	ImageView			cubeImageViews[ 6 ];
	ImageView			cubeDepthImageViews[ 6 ];
	ImageView			diffuseIblImageViews[ 6 ];
	Image				gBufferLayerImage;
	Image				depthStencilImage;
	ImageView			depthImageView;
	ImageView			stencilImageView;
	GpuBufferView		surfParmPartitions[ MaxViews ]; // "View" is used in two ways here: view of data, and view of scene

	// Code images
	ImageView			mainColorResolvedImageView;
	ImageView			depthResolvedImageView;
	ImageView			stencilResolvedImageView;
	Image				shadowMapImage[ MaxShadowViews ];
	Image				mainColorResolvedImage;
	Image				tempColorImage;
	Image				tempWritebackImage;
	Image				depthStencilResolvedImage;

	// Data images
	ImageArray			gpuImages2D;
	ImageArray			gpuImagesCube;
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

private:
	using committedLightsArray_t	= Array<lightBufferObject_t, MaxLights>;
	using materialBufferArray_t		= Array<materialBufferObject_t, MaxMaterials>;

	static const uint32_t				ShadowMapWidth = 1024;
	static const uint32_t				ShadowMapHeight = 1024;
	static const uint32_t				OutlineStencilBit = 0x01;

	renderConfig_t						config;
	RenderView							views[ MaxViews ];
	RenderView*							activeViews[ MaxViews ];
	RenderView*							renderViews[ Max3DViews ];
	RenderView*							shadowViews[ MaxShadowViews ];
	RenderView*							view2Ds[ Max2DViews ];
	ImageProcess*						resolve = nullptr;
	ImageProcess*						diffuseIBL[ 6 ] = {};
	ImageProcess*						pingPongQueue[2] = {};
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
	
	// Render context
	RenderContext						renderContext;
	GfxContext							gfxContext;
	ComputeContext						computeContext;
	UploadContext						uploadContext;
	ComputeState						particleState;
	GpuSemaphore						uploadFinishedSemaphore;

	// Shader resources
	GeometryContext						geometry;
	ResourceContext						resources;
	GpuBuffer							textureStagingBuffer;
	materialBufferArray_t				materialBuffer;
	committedLightsArray_t				committedLights;

	FrameBuffer							shadowMap[ MaxShadowMaps ];
	FrameBuffer							mainColor;
	FrameBuffer							cubeMapFrameBuffer[ 6 ];
	FrameBuffer							diffuseIblFrameBuffer[ 6 ];
	FrameBuffer							mainColorResolved;
	FrameBuffer							tempColor;

	uint32_t							shadowCount = 0;

	// Init/Shutdown
	void								InitApi();
	void								InitShaderResources();
	void								AssignBindSetsToGpuProgs();
	void								InitConfig();
	void								InitImGui( RenderView& view );
	void								ShutdownImGui();
	void								ShutdownShaderResources();
	void								Destroy();
	void								RecreateSwapChain();

	// API Resource Functions
	void								CreateSyncObjects();
	void								CreateFramebuffers();
	void								DestroyFramebuffers();

	// Draw Frame
	void								CommitModel( RenderView& view, const Entity& ent );
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