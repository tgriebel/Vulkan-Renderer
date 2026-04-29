#pragma once
#include "../globals/common.h"
#include "../render_resources/imageArray.h"
#include "../asset_types/image.h"

struct renderConstants_t
{
	Image*			redImage;
	Image*			greenImage;
	Image*			blueImage;
	Image*			whiteImage;
	Image*			blackImage;
	Image*			lightGreyImage;
	Image*			darkGreyImage;
	Image*			brownImage;
	Image*			cyanImage;
	Image*			yellowImage;
	Image*			purpleImage;
	Image*			orangeImage;
	Image*			pinkImage;
	Image*			goldImage;
	Image*			albImage;
	Image*			nmlImage;
	Image*			rghImage;
	Image*			mtlImage;
	Image*			defaultImage;
	Image*			defaultImageCube;
	ImageArray		defaultImageArray;
	ImageArray		defaultImageCubeArray;
};
