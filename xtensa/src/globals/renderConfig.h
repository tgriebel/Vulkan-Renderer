#pragma once

enum imageSamples_t : uint8_t;

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
	bool			useImgui;
	bool			bloom;
	bool			autoExposure;
	bool			chromaticAberration;
	bool			screenshot;
	bool			gaussianBlur;
	bool			ssao;
	bool			dof;
	bool			shadows;
	bool			computeBrdfLut;
	bool			computeNoiseImage;
	bool			rtReflections;
	bool			rayTracingEnabled;
};