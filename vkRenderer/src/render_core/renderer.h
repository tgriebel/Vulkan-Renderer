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

#include "../render_state/cmdContext.h"
#include "../render_binding/bufferObjects.h"
#include "../render_binding/imageSampler.h"
#include "../render_core/renderview.h"
#include "../render_core/renderResource.h"

#include "../render_tasks/RenderTask.h"
#include "../render_tasks/imageShaderTask.h"

class Window;
class SwapChain;
class Scene;

#ifdef USE_VULKAN
using renderPassMap_t = std::unordered_map<uint64_t, VkRenderPass>;
#endif
using pipelineMap_t = std::unordered_map<uint64_t, pipelineObject_t>;
using bindSetMap_t = std::unordered_map<uint64_t, ShaderBindSet>;

#define USE_OPENGL_CONVENTIONS 1

#if defined( USE_IMGUI )
extern imguiControls_t g_imguiControls;
#endif

extern Window						g_window;
extern SwapChain					g_swapChain;

extern renderConstants_t	rc;

struct renderConfig_t
{
	imageSamples_t	mainColorSubSamples;
	const char*		cubemapName;
	bool			present;
	bool			useCubeViews;
	bool			writeCubeViews;
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
	uint64_t				frameNumber = 0;
	float					deltaTimeMs = 0.0f;

public:
	ShaderBindParms*		globalParms;

	// Memory
	AllocatorMemory			scratchMemory;
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

	inline uint64_t			FrameNumber() const { return frameNumber; }

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
private:
	uint32_t				outputImageCount = 0;

	Image* NextImage()
	{
		assert( outputImageCount < MaxFrameImages );
		return &gpuOutput2D[ outputImageCount++ ];
	}

public:
	GpuBuffer				globalConstants;
	GpuBuffer				surfParms;
	GpuBuffer				materialBuffers;
	GpuBuffer				lightParms;
	GpuBuffer				particleBuffer;
	GpuBuffer				defaultUniformBuffer;

	ImageSampler			bilinearSamplerWrap;
	ImageSampler			bilinearSamplerClamp;

	// TODO: move view-specific data
	GpuBuffer				viewParms;
	Image*					mainColorImage;
	Image*					cubeFbColorImage;
	Image*					cubeFbDepthImage;
	Image*					gBufferLayerImage;
	Image*					depthStencilImage;
	ImageView				depthImageView;
	ImageView				stencilImageView;
	GpuBufferView			surfParmPartitions[ MaxViews ]; // "View" is used in two ways here: view of data, and view of scene

	// Code images
	ImageView				depthResolvedImageView;
	ImageView				stencilResolvedImageView;
	Image*					shadowMapImage[ MaxShadowViews ];
	Image*					mainColorResolvedImage;
	Image*					bloom;
	Image*					blurredImage;
	Image*					tempColorImage;
	Image*					previousLum;
	Image*					currentLum;
	Image*					depthStencilResolvedImage;
	Image					gpuOutput2D[ MaxFrameImages ];

	// Data images
	ImageArray				gpuImages2D;
	ImageArray				gpuImagesCube;

	void RegisterOutputImages()
	{
		outputImageCount = 0;

		mainColorImage = NextImage();
		cubeFbColorImage = NextImage();
		cubeFbDepthImage = NextImage();
		gBufferLayerImage = NextImage();
		depthStencilImage = NextImage();

		for( uint32_t i = 0; i < MaxShadowViews; ++i ) {
			shadowMapImage[ i ] = NextImage();
		}
		mainColorResolvedImage = NextImage();
		blurredImage = NextImage();
		bloom = NextImage();
		tempColorImage = NextImage();
		previousLum = NextImage();
		currentLum = NextImage();
		depthStencilResolvedImage = NextImage();
	}

	uint32_t OutputImageCount()
	{
		return outputImageCount;
	}
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

	// Debug
	uint32_t							OutputImageCount();
	const Image*						FindOutputImage( const uint32_t id );

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
	uint32_t							viewCount;
	uint32_t							activeViewCount;

	RenderSchedule						schedule;

	// Timers
	Timer								frameTimer;

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
	void								BeginUploadCommands( UploadContext& uploadContext );
	void								EndUploadCommands( UploadContext& uploadContext );
#ifdef USE_VULKAN
	void								CopyGpuBuffer( CommandContext* cmdCommand, GpuBuffer& srcBuffer, GpuBuffer& dstBuffer, VkBufferCopy copyRegion );
#endif

	void								UploadAssets();
	void								UpdateTextureData( CommandContext* cmdCommand );
	void								UploadTextures( CommandContext* cmdCommand );
	void								UpdateGpuMaterials();
	void								UploadModelsToGPU( CommandContext* cmdCommand );
	void								UpdateBindSets();
	void								UpdateBuffers();
	void								UpdateFrameDescSet();
	void								BuildPipelines();
};
