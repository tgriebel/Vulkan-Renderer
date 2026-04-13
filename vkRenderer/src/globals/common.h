#pragma once

#define GLFW_INCLUDE_VULKAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef near
#undef far
#include <GLFW/glfw3.h>

#define USE_VULKAN
#define USE_VULKAN_RTX
#define USE_IMGUI
#define USE_GLFW
#define USE_TINYFD

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

#include <GfxCore/math/vector.h>
#include <GfxCore/image/color.h>
#include <GfxCore/primitives/geom.h>
#include <GfxCore/acceleration/aabb.h>

#include <SysCore/handle.h>
#include <SysCore/common.h>
#include <SysCore/array.h>
#include <SysCore/timer.h>

#include "../asset_types/material.h"
#include "../scene/camera.h"

#ifdef NDEBUG
const bool EnableValidationLayers = true;
#else
const bool EnableValidationLayers = true;
#endif

const bool		ValidateVerbose					= false;
const bool		ValidateWarnings				= true;
const bool		ValidateErrors					= true;

const uint32_t	DescriptorPoolMaxUniformBuffers	= 1024;
const uint32_t	DescriptorPoolMaxStorageBuffers	= 1024;
const uint32_t	DescriptorPoolMaxSamplers		= 16;
const uint32_t	DescriptorPoolMaxImages			= 1024;
const uint32_t	DescriptorPoolMaxComboImages	= 8192; // Must account for bindless arrays (MaxImageDescriptors) * MaxFrameStates * num bind set instances
const uint32_t	DescriptorPoolMaxSets			= ( DescriptorPoolMaxUniformBuffers + DescriptorPoolMaxStorageBuffers + \
													DescriptorPoolMaxImages + DescriptorPoolMaxComboImages + DescriptorPoolMaxSamplers );
const uint32_t	MaxImageDescriptors				= 128;
const uint32_t	MaxLights						= 128;
const uint32_t	MaxParticles					= 1024;
const uint32_t	MaxShadowMaps					= 6;
const uint32_t	MaxShadowViews					= MaxShadowMaps;
const uint32_t	MaxFrameImages					= 32;
const uint32_t	MaxMipMaps						= 16;
const uint32_t	Max2DViews						= 2;
const uint32_t	Max3DViews						= 7;
const uint32_t	MaxViews						= ( MaxShadowViews + Max3DViews + Max2DViews );
const uint32_t	MaxModels						= 1024;
const uint32_t	MaxVertices						= 0x000FFFFF;
const uint32_t	MaxIndices						= 0x000FFFFF;
const uint32_t	MaxSurfaces						= MaxModels;
const uint32_t	MaxSurfacesDescriptors			= 1;
const uint32_t	MaxMaterials					= 256;
const uint32_t	MaxCodeImages					= 8;
const uint64_t	MaxSharedMemory					= MB( 1024 );
const uint64_t	MaxLocalMemory					= MB( 1024 );
const uint64_t	MaxScratchMemory				= MB( 256 );
const uint64_t	MaxFrameBufferMemory			= GB( 2 );
const uint64_t	MaxTexturingUploadMemory		= MB( 300 );
const uint64_t	MaxGeometryUploadMemory			= MB( 16 );
const uint32_t	MaxFrameStates					= 3;
const uint64_t	MaxTimeStampQueries				= 16;
const uint64_t	MaxOcclusionQueries				= 16;
const uint32_t	DefaultDisplayWidth				= 1280;
const uint32_t	DefaultDisplayHeight			= 720;
const bool		ForceDisableMSAA				= false;

const std::string ModelPath = ".\\models\\";
const std::string TexturePath = ".\\textures\\";
const std::string CodeAssetPath = ".\\code_assets\\";
const std::string ScreenshotPath = "..\\screenshots\\";
const std::string ScenePath = ".\\scenes\\";
const std::string MaterialPath = ".\\materials\\";
const std::string BakePath = ".\\baked\\";
const std::string BakedModelExtension = ".mdl.bin";
const std::string BakedTextureExtension = ".img.bin";
const std::string BakedMaterialExtension = ".mtl.bin";

uint32_t Hash( const uint8_t* bytes, const uint32_t sizeBytes );

typedef void ( *debugMenuFuncPtr )( );

class Renderer;
class Serializer;
class Image;
class GpuProgram;

template<class AssetType>
class Asset;

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

struct imguiImageCallbackData_t
{
	Asset<GpuProgram>*	progAsset;
	uint32_t			permSet;
	const Image*		image;
	float				x;
	float				y;
	float				width;
	float				height;
};

struct imguiControls_t
{
	float				heightMapHeight;
	float				roughnessScale;
	float				roughnessBias;
	float				metalnessScale;
	float				metalnessBias;
	float				shadowStrength;
	float				toneMapColor[ 4 ];
	bool				bloomEnable;
	float				bloomBlendWeight;
	bool				autoExposureEnable;
	float				exposureMidGray;
	float				exposureAdaptation;
	float				exposureWhitePoint;
	float				exposureDarkLimit;
	float				dofFocalDepth;
	float				dofFocalRange;
	bool				dofEnable;
	int32_t				dbgImageId;
	int32_t				selectedFrameBufferImageId;
	int32_t				selectedEntityId;
	bool				rebuildShaders;
	hdl_t				shaderHdl;
	bool				raytraceScene;
	bool				rasterizeScene;
	bool				rebuildRaytraceScene;
	bool				openModelImportFileDialog;
	bool				openSceneFileDialog;
	bool				reloadScene;
	bool				captureScreenshot;
	bool				isTextured;
	vec3f				selectedModelOrigin;
};
#endif
