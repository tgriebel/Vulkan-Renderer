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
#include "../window.h"

#include <tomtendo/interface.h>

static const uint32_t EmuInstances = 4;
static uint64_t lastFrame[ EmuInstances ] = {};
static bool paused[ EmuInstances ] = { false, true, true, true };
static const char* textureBuffers[ EmuInstances ] = { "CODE_COLOR_0", "CODE_COLOR_1", "CODE_COLOR_2", "CODE_COLOR_3" };
static Tomtendo::Emulator nes[ EmuInstances ];
static Tomtendo::config_t nesCfg;
static Tomtendo::wtFrameResult fr;

struct bind_t
{
	key_t					key;
	Tomtendo::ButtonFlags	btn;
};

static const uint32_t ButtonCount = 8;

static bind_t ControllerBinds[ ButtonCount ] =
{
	bind_t{ KEY_H, Tomtendo::ButtonFlags::BUTTON_START },
	bind_t{ KEY_G, Tomtendo::ButtonFlags::BUTTON_SELECT },
	bind_t{ KEY_J, Tomtendo::ButtonFlags::BUTTON_B },
	bind_t{ KEY_K, Tomtendo::ButtonFlags::BUTTON_A },
	bind_t{ KEY_A, Tomtendo::ButtonFlags::BUTTON_LEFT },
	bind_t{ KEY_D, Tomtendo::ButtonFlags::BUTTON_RIGHT },
	bind_t{ KEY_W, Tomtendo::ButtonFlags::BUTTON_UP },
	bind_t{ KEY_S, Tomtendo::ButtonFlags::BUTTON_DOWN },
};

extern Window g_window;

void CopyFrameBuffer( Tomtendo::wtFrameResult& fr, hdl_t texHandle )
{
	Image& texture = g_assets.textureLib.Find( texHandle )->Get();

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

void NesScene::Init()
{
	std::wstring filePaths[ EmuInstances ] =
	{
		L"C:\\Users\\thoma\\source\\repos\\nesEmu\\wintendo\\wintendoApp\\Games\\Super Mario Bros.nes",
		L"C:\\Users\\thoma\\source\\repos\\nesEmu\\wintendo\\wintendoApp\\Games\\Super C.nes",
		L"C:\\Users\\thoma\\source\\repos\\nesEmu\\wintendo\\wintendoApp\\Games\\Ninja Gaiden.nes",
		L"C:\\Users\\thoma\\source\\repos\\nesEmu\\wintendo\\wintendoApp\\Games\\Metroid.nes"
	};
	nesCfg = Tomtendo::DefaultConfig();

	for( uint32_t i = 0; i < EmuInstances; ++i )
	{
		nes[i].Boot( filePaths[i] );
		nes[i].SetConfig( nesCfg );

		for ( uint32_t k = 0; k < ButtonCount; ++k )
		{
			using namespace Tomtendo;
			nes[i].input.BindKey( ControllerBinds[ k ].key, ControllerId::CONTROLLER_0, ControllerBinds[ k ].btn );
		}
	}
}


void NesScene::Shutdown()
{
	for ( uint32_t i = 0; i < EmuInstances; ++i )
	{
	}
}


void NesScene::Update()
{
	static bool emulatorRunning = true;
	if( emulatorRunning )
	{
		for ( uint32_t i = 0; i < EmuInstances; ++i )
		{
			if ( !nes[i].RunEpoch( DeltaNano() ) ) {
				emulatorRunning = false;
				break;
			}

			for ( uint32_t k = 0; k < ButtonCount; ++k )
			{
				if ( g_window.input.IsKeyPressed( ControllerBinds[ k ].key ) ) {
					nes[i].input.StoreKey( ControllerBinds[ k ].key );
				} else {
					nes[i].input.ReleaseKey( ControllerBinds[ k ].key );
				}
			}

			nes[i].GetFrameResult( fr );
			if ( fr.currentFrame > lastFrame[i] )
			{
				lastFrame[i] = fr.currentFrame;
				CopyFrameBuffer( fr, AssetLibImages::Handle( textureBuffers[i] ) );
			}
		}
	}
}