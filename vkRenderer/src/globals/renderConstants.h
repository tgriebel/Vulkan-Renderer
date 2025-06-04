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
#include "../globals/common.h"
#include <GfxCore/asset_types/texture.h>

struct renderConstants_t
{
	Image*		redImage;
	Image*		greenImage;
	Image*		blueImage;
	Image*		whiteImage;
	Image*		blackImage;
	Image*		lightGreyImage;
	Image*		darkGreyImage;
	Image*		brownImage;
	Image*		cyanImage;
	Image*		yellowImage;
	Image*		purpleImage;
	Image*		orangeImage;
	Image*		pinkImage;
	Image*		goldImage;
	Image*		albImage;
	Image*		nmlImage;
	Image*		rghImage;
	Image*		mtlImage;
	Image*		defaultImage;
	Image*		defaultImageCube;
};