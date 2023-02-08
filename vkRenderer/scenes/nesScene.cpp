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

#include "nesScene.h"

#include <tomtendo/interface.h>

static Tomtendo::Emulator* nes = new Tomtendo::Emulator();
static Tomtendo::config_t nesCfg;

void NesScene::Init()
{
	std::wstring filePath = L"C:\\Users\\thoma\\source\\repos\\nesEmu\\wintendo\\wintendoApp\\Games\\Legend of Zelda.nes";
	Tomtendo::InitConfig( nesCfg );
	nes->Boot( filePath );
	nes->SetConfig( nesCfg );
}

void NesScene::Update( const std::chrono::nanoseconds delta )
{
	static uint32_t lastFrame = 0;
	static bool emulatorRunning = true;
	if( emulatorRunning )
	{
		if ( !nes->RunEpoch( delta ) ) {
			emulatorRunning = false;
		}

		Tomtendo::wtFrameResult fr;
		nes->GetFrameResult( fr );
		
		if( fr.currentFrame > lastFrame )
		{
			lastFrame = fr.currentFrame;

			Texture& texture = gAssets.textureLib.Find( "CODE_COLOR" )->Get();

			const uint32_t width = fr.frameBuffer->GetWidth();
			const uint32_t height = fr.frameBuffer->GetHeight();

			for ( uint32_t y = 0; y < height; ++y )
			{
				for ( uint32_t x = 0; x < width; ++x )
				{
					const uint32_t pixelIx = y * width + x;
					Tomtendo::Pixel pixel = fr.frameBuffer->Get( pixelIx );

					texture.bytes[ pixelIx * 4 + 0 ] = pixel.rgba.red;
					texture.bytes[ pixelIx * 4 + 1 ] = pixel.rgba.green;
					texture.bytes[ pixelIx * 4 + 2 ] = pixel.rgba.blue;
					texture.bytes[ pixelIx * 4 + 3 ] = pixel.rgba.alpha;
				}
				texture.dirty = true;
			}
		}
	}
}