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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define USE_VULKAN
#define USE_IMGUI

#include <gfxcore/acceleration/aabb.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <utility>
#include <set>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>
#include <ctime>
#include <ratio>
#include <unordered_map>
#include <stdlib.h>
#include <math.h>
#include <atomic>

#include <gfxcore/math/vector.h>
#include <gfxcore/core/handle.h>
#include <gfxcore/image/color.h>
#include <gfxcore/asset_types/material.h>
#include <gfxcore/primitives/geom.h>
#include <gfxcore/scene/camera.h>
#include <gfxcore/asset_types/gpuProgram.h>
#include <syscore/common.h>
#include <sysCore/array.h>
#include <syscore/timer.h>

#ifdef NDEBUG
const bool EnableValidationLayers = true;
#else
const bool EnableValidationLayers = true;
#endif

const bool		ValidateVerbose					= false;
const bool		ValidateWarnings				= true;
const bool		ValidateErrors					= true;

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1000;
const uint32_t	DescriptorPoolMaxStorageBuffers	= 1000;
const uint32_t	DescriptorPoolMaxSamplers		= 3;
const uint32_t	DescriptorPoolMaxImages			= 1000;
const uint32_t	DescriptorPoolMaxComboImages	= 1000;
const uint32_t	DescriptorPoolMaxSets			= ( DescriptorPoolMaxUniformBuffers + DescriptorPoolMaxStorageBuffers + DescriptorPoolMaxImages + DescriptorPoolMaxComboImages );
const uint32_t	MaxImageDescriptors				= 100;
const uint32_t	MaxLights						= 128;
const uint32_t	MaxParticles					= 1024;
const uint32_t	MaxShadowMaps					= 6;
const uint32_t	MaxShadowViews					= MaxShadowMaps;
const uint32_t	Max2DViews						= 2;
const uint32_t	Max3DViews						= 7;
const uint32_t	MaxViews						= ( MaxShadowViews + Max3DViews + Max2DViews );
const uint32_t	MaxModels						= 1000;
const uint32_t	MaxVertices						= 0x000FFFFF;
const uint32_t	MaxIndices						= 0x000FFFFF;
const uint32_t	MaxSurfaces						= MaxModels;
const uint32_t	MaxSurfacesDescriptors			= 1;
const uint32_t	MaxMaterials					= 256;
const uint32_t	MaxCodeImages					= 3;
const uint64_t	MaxSharedMemory					= MB( 1024 );
const uint64_t	MaxLocalMemory					= MB( 1024 );
const uint64_t	MaxScratchMemory				= MB( 256 );
const uint64_t	MaxFrameBufferMemory			= GB( 2 );
const uint32_t	MaxFrameStates					= 3;
const uint64_t	MaxTimeStampQueries				= 12;
const uint64_t	MaxOcclusionQueries				= 12;
const uint32_t	DefaultDisplayWidth				= 1280;
const uint32_t	DefaultDisplayHeight			= 720;
const bool		ForceDisableMSAA				= false;

const std::string ModelPath = ".\\models\\";
const std::string TexturePath = ".\\textures\\";
const std::string CodeAssetPath = ".\\code_assets\\";
const std::string ScreenshotPath = "..\\screenshots\\";
const std::string MaterialPath = ".\\materials\\";
const std::string BakePath = ".\\baked\\";
const std::string BakedModelExtension = ".mdl.bin";
const std::string BakedTextureExtension = ".img.bin";
const std::string BakedMaterialExtension = ".mtl.bin";

uint32_t Hash( const uint8_t* bytes, const uint32_t sizeBytes );

using ImageArray = Array<const Image*, MaxImageDescriptors>;

typedef void ( *debugMenuFuncPtr )( );

class Renderer;
class Serializer;


enum renderFlags_t : uint32_t
{
	NONE		= 0,
	HIDDEN		= ( 1 << 0 ),
	NO_SHADOWS	= ( 1 << 1 ),
	WIREFRAME	= ( 1 << 2 ),	// FIXME: this needs to make a unique pipeline object if checked
	DEBUG_SOLID	= ( 1 << 3 ),	// "
	SKIP_OPAQUE	= ( 1 << 4 ),
	COMMITTED	= ( 1 << 5 ),
};


enum class swapBuffering_t : uint8_t
{
	SINGLE_FRAME,
	MULTI_FRAME,
};


#if defined( USE_IMGUI )
struct imguiControls_t
{
	float		heightMapHeight;
	float		roughness;
	float		shadowStrength;
	float		toneMapColor[ 4 ];
	float		dofFocalDepth;
	float		dofFocalRange;
	bool		dofEnable;
	int			dbgImageId;
	int			selectedEntityId;
	bool		rebuildShaders;
	hdl_t		shaderHdl;
	bool		raytraceScene;
	bool		rasterizeScene;
	bool		rebuildRaytraceScene;
	bool		openModelImportFileDialog;
	bool		openSceneFileDialog;
	bool		reloadScene;
	bool		captureScreenshot;
	vec3f		selectedModelOrigin;
};
#endif