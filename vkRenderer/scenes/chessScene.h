#pragma once
#include "../src/globals/common.h"
#include "../src/globals/render_util.h"
#include "../src/io/io.h"
#include "../src/app/window.h"
#include "../src/app/input.h"
#include "Chess.h"
#include <syscore/timer.h>
#include "../src/scene/entity.h"
#include "../src/scene/sceneBase.h"
#include "../src/asset_types/gpuProgram.h"
#include "../src/asset_types/model.h"
#include "../src/io/io.h"
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
	ChessEngine		chessEngine;
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
