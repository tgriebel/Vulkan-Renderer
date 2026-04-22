#pragma once

#include <cstdint>

class GpuProgram;
class Image;

template<class AssetType>
class Asset;

struct imguiImageCallbackData_t
{
	Asset<GpuProgram>* progAsset;
	uint32_t			permSet;
	const Image*		image;
	float				x;
	float				y;
	float				width;
	float				height;
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
