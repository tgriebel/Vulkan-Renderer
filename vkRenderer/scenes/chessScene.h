#pragma once
#include "../src/globals/common.h"
#include "../src/globals/render_util.h"
#include "../io.h"
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
	Entity*			selectedEntity = nullptr;
	Entity*			movePieceId = nullptr;

	std::vector< moveAction_t >	actions;
	std::vector< uint32_t >		pieceEntities;
	std::vector< uint32_t >		glowEntities;
	std::vector< uint32_t >		boundEntities;
public:
	ChessScene() : Scene()
	{
	}

	void Update( const float dt ) override;
	void Init() override;
};