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
#include "../src/globals/common.h"
#include "../src/globals/render_util.h"
#include "../src/io/io.h"
#include "../window.h"
#include "../input.h"
#include <chess.h>
#include <commands.h>
#include <syscore/timer.h>
#include <scene/entity.h>
#include <scene/scene.h>
#include <resource_types/gpuProgram.h>
#include <resource_types/model.h>
#include <io/io.h>
#include <algorithm>

class PieceEntity : public Entity {
public:
	PieceEntity( const char file, const char rank ) : Entity(),
		file( file ),
		rank( rank ),
		handle( NoPiece ) { }
	char			file;	// TODO: remove
	char			rank;	// TODO: remove
	pieceHandle_t	handle;
};

class BoundEntity : public Entity {
public:
	BoundEntity() : Entity(),
		pieceId( ~0x0 ) { }
	uint32_t	pieceId;
};

class ChessScene : public Scene
{
private:
	gameConfig_t	cfg;
	Chess			chessEngine;
	Entity*			movePieceId = nullptr;

	std::vector< moveAction_t >	actions;
	std::vector< uint32_t >		pieceEntities;
	std::vector< uint32_t >		glowEntities;
	std::vector< uint32_t >		boundEntities;
public:
	ChessScene() : Scene()
	{
	}

	void Update() override;
	void Init() override;
};