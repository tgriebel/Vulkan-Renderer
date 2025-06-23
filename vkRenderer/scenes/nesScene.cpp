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

#include <windows.h> 
#include <stdio.h>

#define IMPORT_WIN
#include "tomtendoCore.h"

//using namespace Tomtendo;

static const uint32_t EmuInstances = 4;
static uint64_t lastFrame[ EmuInstances ] = {};
static bool paused[ EmuInstances ] = { false, true, true, true };
static const char* textureBuffers[ EmuInstances ] = { "CODE_COLOR_0", "CODE_COLOR_1", "CODE_COLOR_2", "CODE_COLOR_3" };
static Tomtendo::Emulator* nes[ EmuInstances ];
static Tomtendo::config_t nesCfg;
static Tomtendo::wtFrameResult fr;

BOOL fRunTimeLinkSuccess = FALSE;
HINSTANCE hinstLib = nullptr;

Tomtendo::runtimeDllInterface_t tomtendo{};

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
	Asset<Image>* imageAsset = g_assets.textureLib.Find( texHandle );
	Image& texture = imageAsset->Get();

	const uint32_t width = fr.frameBuffer->GetWidth();
	const uint32_t height = fr.frameBuffer->GetHeight();

	ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

	for ( uint32_t y = 0; y < height; ++y )
	{
		for ( uint32_t x = 0; x < width; ++x )
		{
			const uint32_t pixelIx = y * width + x;
			Tomtendo::Pixel pixel = fr.frameBuffer->Get( pixelIx );

			rgba8_t rgba {};
			rgba.r = pixel.rgba.alpha;
			rgba.g = pixel.rgba.blue;
			rgba.b = pixel.rgba.green;
			rgba.a = pixel.rgba.red;

			imageBuffer->SetPixel( x, y, rgba );
		}
	}
	imageAsset->QueueUpload();
}

void NesScene::Init()
{
	// Get a handle to the DLL module.
	hinstLib = LoadLibrary( TEXT( "scenes/wintendoCore.dll" ) );

	if ( hinstLib == nullptr ) {
		return;
	}
	fRunTimeLinkSuccess = TRUE;

	std::wstring filePaths[ EmuInstances ] =
	{
		L"scenes/Super Mario Bros.nes",
		L"scenes/Super C.nes",
		L"scenes/Ninja Gaiden.nes",
		L"scenes/Metroid.nes"
	};

	Tomtendo::LoadDllInterface( &tomtendo, hinstLib );

	std::cout << tomtendo.ScreenHeight() << std::endl;

	nesCfg = tomtendo.DefaultConfig();

	for( uint32_t i = 0; i < EmuInstances; ++i )
	{
		tomtendo.CreateEmulatorInstance( &nes[ i ] );

		tomtendo.Boot( nes[ i ], filePaths[ i ].c_str(), 0x10000 );
		tomtendo.SetConfig( nes[ i ], nesCfg );

		for ( uint32_t k = 0; k < ButtonCount; ++k )
		{
			using namespace Tomtendo;
			tomtendo.BindKey( &nes[ i ]->input, ControllerBinds[ k ].key, ControllerId::CONTROLLER_0, ControllerBinds[ k ].btn );
		}
	}
}


void NesScene::Shutdown()
{
	if( fRunTimeLinkSuccess == FALSE ) {
		return;
	}

	for ( uint32_t i = 0; i < EmuInstances; ++i )
	{
		tomtendo.DestroyEmulatorInstance( &nes[ i ] );
	}
	BOOL fFreeResult = FreeLibrary( hinstLib );
}


void NesScene::Update()
{
	static bool emulatorRunning = true;
	if( emulatorRunning )
	{
		for ( uint32_t i = 0; i < EmuInstances; ++i )
		{
			if ( !tomtendo.RunEpoch( nes[ i ], DeltaNano() ) ) {
				emulatorRunning = false;
				break;
			}

			for ( uint32_t k = 0; k < ButtonCount; ++k )
			{
				if ( g_window.input.IsKeyPressed( ControllerBinds[ k ].key ) ) {
					tomtendo.StoreKey( &nes[ i ]->input, ControllerBinds[ k ].key );
				} else {
					tomtendo.ReleaseKey( &nes[ i ]->input, ControllerBinds[ k ].key );
				}
			}

			tomtendo.GetFrameResult( nes[ i ], fr );
			if ( fr.currentFrame > lastFrame[i] )
			{
				lastFrame[i] = fr.currentFrame;
				CopyFrameBuffer( fr, AssetLibImages::Handle( textureBuffers[i] ) );
			}
		}
	}
}