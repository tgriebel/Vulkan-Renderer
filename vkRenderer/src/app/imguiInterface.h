#pragma once

#include <cstdint>

class GpuProgram;
class Image;

template<class AssetType>
class Asset;

struct imageViewerCallbackData_t
{
	const Image*		image;
	uint32_t			pixelX;
	uint32_t			pixelY;
	float				x;
	float				y;
	float				width;
	float				height;
	vec4f				tint;			// Per-channel scale: R, G, B, A
	float				rangeMin;		// RGBA values below this clamp are displayed black
	float				rangeMax;		// RGBA values above this clamp are displayed white
	uint32_t			flags;			// Bit 0: cube image. Bit 1: Apply sRGB gamma
	uint32_t			mipLevel;
	uint32_t			layer;
	uint32_t			msaaSampleIndex;	// ~0u: average all samples
};


struct imageViewerStatistics_t
{
	vec4f	minSample;
	vec4f	maxSample;
};


struct postProcessControls_t
{
	float				toneMapColor[ 4 ];
	float				bloomBlendWeight;
	float				exposureMidGray;
	float				exposureAdaptation;
	float				exposureWhitePoint;
	float				exposureDarkLimit;
	float				dofFocalDepth;
	float				dofFocalRange;
	float				caIntensity;
	bool				bloomEnable;
	bool				autoExposureEnable;
	bool				dofEnable;
	bool				caEnable;
};


struct pbrControls_t
{
	float	roughnessScale;
	float	roughnessBias;
	float	metalnessScale;
	float	metalnessBias;
	bool	useDiffuseIBL;
	bool	useSpecularIBL;

	int32_t	debugLightingMode;
};


struct imguiControls_t
{
	postProcessControls_t	postProcess;
	pbrControls_t			pbr;

	float					heightMapHeight;
	float					shadowStrength;
	int32_t					dbgImageId;
	int32_t					selectedFrameBufferImageId;
	int32_t					selectedEntityId;
	vec3f					selectedModelOrigin;
	hdl_t					shaderHdl;
	bool					rebuildShaders;
	bool					raytraceScene;
	bool					rasterizeScene;
	bool					rebuildRaytraceScene;
	bool					openModelImportFileDialog;
	bool					openSceneFileDialog;
	bool					reloadScene;
	bool					captureScreenshot;
	bool					isTextured;
};
