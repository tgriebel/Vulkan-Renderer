/*
* MIT License
*
* Copyright( c ) 2023-2026 Thomas Griebel
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

//
// Chess.h — Chess engine public header
//
// Self-contained chess logic: board state, piece movement, move
// validation, check/checkmate detection, castling, en passant,
// and pawn promotion. No external dependencies beyond the
// standard library.
//
// Consumers include this single header and link against the
// Chess static library.
//

#include <assert.h>
#include <string>
#include <vector>
#include <limits>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>


// ============================================================
// Constants
// ============================================================

static const int BoardSize		= 8;
static const int TeamCount		= 2;
static const int TeamPieceCount	= 16;
static const int PieceCount		= 32;
typedef int pieceHandle_t;
static const pieceHandle_t NoPiece = -1;
static const pieceHandle_t OffBoard = -2;

class ChessEngine;


// ============================================================
// Enums
// ============================================================

enum resultCode_t
{
	ERROR_RANGE_START				= -101,
	RESULT_INPUT_INVALID_COMMAND	= ERROR_RANGE_START,
	RESULT_INPUT_INVALID_PIECE,
	RESULT_INPUT_INVALID_FILE,
	RESULT_INPUT_INVALID_RANK,
	RESULT_INPUT_INVALID_MOVE,
	RESULT_GAME_INVALID_PIECE,
	RESULT_GAME_INVALID_MOVE,
	RESULT_GAME_COMPLETE,
	ERROR_COUNT						= ( RESULT_GAME_COMPLETE - RESULT_INPUT_INVALID_COMMAND ) + 1,
	RESULT_SUCCESS					= 0,
};

static const char* ErrorMsgs[ ERROR_COUNT ] =
{
	"Invalid command",			// RESULT_INPUT_INVALID_COMMAND
	"Invalid piece",			// RESULT_INPUT_INVALID_PIECE
	"File out of range",		// RESULT_INPUT_INVALID_FILE
	"Rank out of range",		// RESULT_INPUT_INVALID_RANK
	"Invalid move command",		// RESULT_INPUT_INVALID_MOVE
	"Invalid piece selected",	// RESULT_GAME_INVALID_PIECE
	"Invalid move action",		// RESULT_GAME_INVALID_MOVE
	"Game Complete",			// RESULT_GAME_COMPLETE
};

inline const char* GetErrorMsg( const resultCode_t code )
{
	const int index = code - ERROR_RANGE_START;
	return ErrorMsgs[ index ];
}

enum class pieceType_t : int
{
	NONE = -1,
	PAWN = 0,
	ROOK,
	KNIGHT,
	BISHOP,
	KING,
	QUEEN,
	COUNT,
};

enum class teamCode_t : int
{
	NONE = -1,
	WHITE = 0,
	BLACK = 1,
	COUNT
};

enum class moveType_t : int
{
	NONE = -1,

	PAWN_T = 0,
	PAWN_T2X,
	PAWN_KILL_L,
	PAWN_KILL_R,

	KNIGHT_T1L2,
	KNIGHT_T2L1,
	KNIGHT_T1R2,
	KNIGHT_T2R1,
	KNIGHT_B1R2,
	KNIGHT_B2R1,
	KNIGHT_B2L1,
	KNIGHT_B1L2,

	ROOK_L,
	ROOK_R,
	ROOK_T,
	ROOK_B,

	BISHOP_TL,
	BISHOP_TR,
	BISHOP_BR,
	BISHOP_BL,

	KING_TL,
	KING_T,
	KING_TR,
	KING_R,
	KING_BR,
	KING_B,
	KING_BL,
	KING_L,
	KING_CASTLE_L,
	KING_CASTLE_R,

	QUEEN_TL,
	QUEEN_T,
	QUEEN_TR,
	QUEEN_R,
	QUEEN_BR,
	QUEEN_B,
	QUEEN_BL,
	QUEEN_L,

	PAWN_ACTIONS	= ( PAWN_KILL_R - PAWN_T ) + 1,
	KNIGHT_ACTIONS	= ( KNIGHT_B1L2 - KNIGHT_T1L2 ) + 1,
	ROOK_ACTIONS	= ( ROOK_B - ROOK_L ) + 1,
	BISHOP_ACTIONS	= ( BISHOP_BL - BISHOP_TL ) + 1,
	KING_ACTIONS	= ( KING_CASTLE_R - KING_TL ) + 1,
	QUEEN_ACTIONS	= ( QUEEN_L - QUEEN_TL ) + 1,
	MOVE_COUNT		= ( PAWN_ACTIONS + KNIGHT_ACTIONS + ROOK_ACTIONS + BISHOP_ACTIONS + KING_ACTIONS + QUEEN_ACTIONS ),
};

enum callbackEventType_t
{
	PAWN_PROMOTION
};


// ============================================================
// Structs
// ============================================================

struct callbackEvent_t
{
	callbackEventType_t	type;
	pieceType_t			promotionType;
};

struct moveAction_t
{
	moveAction_t()
	{
		x = 0;
		y = 0;
		maxSteps = 0;
		type = moveType_t::NONE;
	}

	moveAction_t( const int x, int const y, const moveType_t type, const int maxSteps ) :
		x( x ), y( y ), type( type ), maxSteps( maxSteps ) {}

	int				x;
	int				y;
	int				maxSteps;
	moveType_t		type;
};

struct command_t
{
	int				x;
	int				y;
	pieceType_t		pieceType;
	int				instance;
	teamCode_t		team;
};

struct team_t
{
	team_t()
	{
		for ( int i = 0; i < TeamPieceCount; ++i )
		{
			pieces[ i ] = 0;
			captured[ i ] = 0;
		}
		for ( int i = 0; i < (int)pieceType_t::COUNT; ++i )
		{
			typeCounts[ i ] = 0;
			captureTypeCounts[ i ] = 0;
		}
		livingCount = 0;
		capturedCount = 0;
	}

	pieceHandle_t	pieces[ TeamPieceCount ];
	pieceHandle_t	captured[ TeamPieceCount ];
	int				livingCount;
	int				capturedCount;
	int				typeCounts[ (int)pieceType_t::COUNT ];
	int				captureTypeCounts[ (int)pieceType_t::COUNT ];
};

struct pieceInfo_t
{
	teamCode_t	team;
	pieceType_t	piece;
	int			instance;
	bool		onBoard;
};

struct gameConfig_t
{
	gameConfig_t() {};

	gameConfig_t( const gameConfig_t& config )
	{
		for ( int i = 0; i < BoardSize; ++i )
		{
			for ( int j = 0; j < BoardSize; ++j )
			{
				board[ i ][ j ] = config.board[ i ][ j ];
			}
		}
	}

	gameConfig_t& operator=( const gameConfig_t& config )
	{
		if ( this == &config )
		{
			return *this;
		}
		for ( int i = 0; i < BoardSize; ++i )
		{
			for ( int j = 0; j < BoardSize; ++j )
			{
				board[ i ][ j ] = config.board[ i ][ j ];
			}
		}
		return *this;
	}

	pieceInfo_t board[ BoardSize ][ BoardSize ];
};

typedef void ( *callback_t )( callbackEvent_t& );


// ============================================================
// Utility declarations
// ============================================================

void GetDefaultConfig( gameConfig_t& defaultCfg );
std::string SquareToString( const ChessEngine& board, const int x, const int y );
std::string TeamCaptureString( const ChessEngine& board, const teamCode_t team );
std::string BoardToString( const ChessEngine& board, const bool printCaptures );
void LoadConfig( const std::string& fileName, gameConfig_t& config );
void LoadHistory( const std::string& fileName, std::vector< std::string >& commands );


// ============================================================
// Move action tables
// ============================================================

static const moveAction_t PawnActions[ (int)moveType_t::PAWN_ACTIONS ] =
{
	moveAction_t( 0, 1, moveType_t::PAWN_T, 1 ),
	moveAction_t( 0, 1, moveType_t::PAWN_T2X, 2 ),
	moveAction_t( -1, 1, moveType_t::PAWN_KILL_L, 1 ),
	moveAction_t( 1, 1, moveType_t::PAWN_KILL_R, 1 )
};

static const moveAction_t RookActions[ (int)moveType_t::ROOK_ACTIONS ] =
{
	moveAction_t( 0, 1, moveType_t::ROOK_T, BoardSize - 1 ),
	moveAction_t( 0, -1, moveType_t::ROOK_B, BoardSize - 1 ),
	moveAction_t( 1, 0, moveType_t::ROOK_R, BoardSize - 1 ),
	moveAction_t( -1, 0, moveType_t::ROOK_L, BoardSize - 1 )
};

static const moveAction_t KnightActions[ (int)moveType_t::KNIGHT_ACTIONS ] =
{
	moveAction_t( -2, -1, moveType_t::KNIGHT_T1L2, 1 ),
	moveAction_t( -1, -2, moveType_t::KNIGHT_T2L1, 1 ),
	moveAction_t( 1, -2, moveType_t::KNIGHT_T1R2, 1 ),
	moveAction_t( 2, -1, moveType_t::KNIGHT_T2R1, 1 ),
	moveAction_t( 2, 1, moveType_t::KNIGHT_B1R2, 1 ),
	moveAction_t( 1, 2, moveType_t::KNIGHT_B2R1, 1 ),
	moveAction_t( -1, 2, moveType_t::KNIGHT_B2L1, 1 ),
	moveAction_t( -2, 1, moveType_t::KNIGHT_B1L2, 1 )
};

static const moveAction_t BishopActions[ (int)moveType_t::BISHOP_ACTIONS ] = 
{
	moveAction_t( -1, -1, moveType_t::BISHOP_TL, BoardSize - 1 ),
	moveAction_t( 1, -1, moveType_t::BISHOP_TR, BoardSize - 1 ),
	moveAction_t( 1, 1, moveType_t::BISHOP_BR, BoardSize - 1 ),
	moveAction_t( -1, 1, moveType_t::BISHOP_BL, BoardSize - 1 )
};

static const moveAction_t KingActions[ (int)moveType_t::KING_ACTIONS ] =
{
	moveAction_t( -1, -1, moveType_t::KING_TL, 1 ),
	moveAction_t( 0, -1, moveType_t::KING_T, 1 ),
	moveAction_t( 1, -1, moveType_t::KING_TR, 1 ),
	moveAction_t( 1, 0, moveType_t::KING_R, 1 ),
	moveAction_t( 1, 1, moveType_t::KING_BR, 1 ),
	moveAction_t( 0, 1, moveType_t::KING_B, 1 ),
	moveAction_t( -1, 1, moveType_t::KING_BL, 1 ),
	moveAction_t( -1, 0, moveType_t::KING_L, 1 ),
	moveAction_t( -2, 0, moveType_t::KING_CASTLE_L, 1 ),
	moveAction_t( 2, 0, moveType_t::KING_CASTLE_R, 1 )
};

static const moveAction_t QueenActions[ (int)moveType_t::QUEEN_ACTIONS ] =
{
	moveAction_t( -1, -1, moveType_t::QUEEN_TL, BoardSize - 1 ),
	moveAction_t( 0, -1, moveType_t::QUEEN_T, BoardSize - 1 ),
	moveAction_t( 1, -1, moveType_t::QUEEN_TR, BoardSize - 1 ),
	moveAction_t( 1, 0, moveType_t::QUEEN_R, BoardSize - 1 ),
	moveAction_t( 1, 1, moveType_t::QUEEN_BR, BoardSize - 1 ),
	moveAction_t( 0, 1, moveType_t::QUEEN_B, BoardSize - 1 ),
	moveAction_t( -1, 1, moveType_t::QUEEN_BL, BoardSize - 1 ),
	moveAction_t( -1, 0, moveType_t::QUEEN_L, BoardSize - 1 )
};


// ============================================================
// Piece classes
// ============================================================

class ChessState;

class Piece
{
protected:
	static const int MaxActions = 10;

	Piece()
	{
		team = teamCode_t::NONE;
		type = pieceType_t::NONE;
		x = -1;
		y = -1;
		numActions = 0;
		moveCount = 0;
		instance = 0;
		handle = NoPiece;
		state = nullptr;
	}

	bool			IsValidAction( const int actionNum ) const;
public:
	Piece( const Piece& src ) { *this = src; }

	Piece& operator=( const Piece& src )
	{
		if ( this != &src )
		{
			this->x = src.x;
			this->y = src.y;
			this->team = src.team;
			this->type = src.type;
			this->moveCount = src.moveCount;
			this->numActions = src.numActions;
			this->handle = src.handle;
			this->state = src.state;
		}
		return *this;
	}

	void			CalculateStep( const int actionNum, int& actionX, int& actionY ) const;
	int				GetStepCount( const int actionNum, const int targetX, const int targetY ) const;
	int				GetActionPath( const int actionNum, moveAction_t path[ BoardSize ] ) const;
	virtual bool	InActionPath( const int actionNum, const int targetX, const int targetY ) const;
	virtual void	Move( const int targetX, const int targetY );
	void			Set( const int targetX, const int targetY );

	bool			HasMoved() const { return ( moveCount > 0 ); }
	int				GetActionCount() const { return numActions; }
	bool			OnBoard() const { return ( state != nullptr ); }

	virtual inline int GetDirection() const { return 1; }

	void RemoveFromPlay()
	{
		Set( -1, -1 );
		state = nullptr;
	}

	int GetActionNum( const moveType_t moveType ) const
	{
		const moveAction_t* actions = GetActions();
		const int actionCount = GetActionCount();
		for ( int i = 0; i < actionCount; ++i )
		{
			if ( actions[ i ].type == moveType )
			{
				return i;
			}
		}
		return -1;
	}

	const moveAction_t& GetAction( const int actionNum ) const {
		return GetActions()[ actionNum ];
	}

	virtual const moveAction_t* GetActions() const = 0;

private:
	void BindBoard( ChessState* state, const pieceHandle_t handle )
	{
		this->state = state;
		this->handle = handle;
	}

public:
	teamCode_t			team;
	pieceType_t			type;
	int					instance;

protected:
	int					x;
	int					y;
	int					moveCount;
	int					numActions;
	pieceHandle_t		handle;

	ChessState*			state;

	friend class ChessEngine;
	friend class ChessState;
};

class Pawn : public Piece
{
public:
	Pawn( const teamCode_t team ) : Piece()
	{
		this->type = pieceType_t::PAWN;
		this->team = team;
		this->numActions = static_cast<int>( moveType_t::PAWN_ACTIONS );
	}

	bool InActionPath( const int actionNum, const int targetX, const int targetY ) const override;
	void Move( const int targetX, const int targetY ) override;
	bool CanPromote() const;

	inline int GetDirection() const {
		return ( team == teamCode_t::WHITE ) ? -1 : 1;
	}

	const moveAction_t* GetActions() const {
		return PawnActions;
	}

private:
	inline bool IsKillAction( const moveType_t type ) const {
		return ( type == moveType_t::PAWN_KILL_L ) || ( type == moveType_t::PAWN_KILL_R );
	}
};


class Rook : public Piece
{
public:
	Rook( const teamCode_t team ) : Piece()
	{
		this->type = pieceType_t::ROOK;
		this->team = team;
		this->numActions = static_cast<int>( moveType_t::ROOK_ACTIONS );
	}

	const moveAction_t* GetActions() const
	{
		return RookActions;
	}
};


class Knight : public Piece
{
public:
	Knight( const teamCode_t team ) : Piece()
	{
		this->type = pieceType_t::KNIGHT;
		this->team = team;
		this->numActions = static_cast<int>( moveType_t::KNIGHT_ACTIONS );
	}

	const moveAction_t* GetActions() const
	{
		return KnightActions;
	}
};


class Bishop : public Piece
{
public:
	Bishop( const teamCode_t team ) : Piece()
	{
		this->type = pieceType_t::BISHOP;
		this->team = team;
		this->numActions = static_cast<int>( moveType_t::BISHOP_ACTIONS );
	}

	const moveAction_t* GetActions() const
	{
		return BishopActions;
	}
};

class King : public Piece
{
public:
	King( const teamCode_t team ) : Piece()
	{
		this->type = pieceType_t::KING;
		this->team = team;
		this->numActions = static_cast<int>( moveType_t::KING_ACTIONS );
	}

	const moveAction_t* GetActions() const
	{
		return KingActions;
	}

	bool InActionPath( const int actionNum, const int actionX, const int actionY ) const override;
};


class Queen : public Piece
{
public:
	Queen( const teamCode_t team ) : Piece()
	{
		this->type = pieceType_t::QUEEN;
		this->team = team;
		this->numActions = static_cast<int>( moveType_t::QUEEN_ACTIONS );
	}

	const moveAction_t* GetActions() const
	{
		return QueenActions;
	}
};


// ============================================================
// ChessState
// ============================================================

class ChessState
{
public:
	ChessState() {}
	ChessState( const ChessState& state ) { CopyState( state ); }
	~ChessState() {}

	bool				IsLegalMove( const Piece* piece, const int targetX, const int targetY ) const;
	inline bool			OnBoard( const int x, const int y ) const;
	inline const Piece* GetPiece( const pieceHandle_t handle ) const;
	inline Piece*		GetPiece( const pieceHandle_t handle );
	const Piece*		GetPiece( const int x, const int y ) const;
	Piece*				GetPiece( const int x, const int y );
	void				SetHandle( const pieceHandle_t pieceHdl, const int x, const int y );
	pieceHandle_t		GetHandle( const int x, const int y ) const;
	pieceInfo_t			GetInfo( const int x, const int y ) const;

	void				CapturePiece( const teamCode_t attacker, Piece* targetPiece );
	bool				FindCheckMate( const teamCode_t team );
	bool				IsOpenToAttackAt( const Piece* targetPiece, const int targetX, const int targetY ) const;
	pieceHandle_t		GetEnpassant( const int targetX, const int targetY ) const;
	inline void			SetEnpassant( const pieceHandle_t handle ) { enpassantPawn = handle; }
	void				PromotePawn( const pieceHandle_t pieceHdl );

private:
	void				CopyState( const ChessState& state );
	void				CountTeamPieces();
private:
	callback_t			callback;
	pieceHandle_t		enpassantPawn;
	Piece*				pieces[ PieceCount ];
	team_t				teams[ TeamCount ];
	pieceHandle_t		grid[ BoardSize ][ BoardSize ]; // (0,0) is top left
	ChessEngine*		game;

	friend class ChessEngine;
};


// ============================================================
// ChessEngine (formerly: Chess)
// ============================================================

class ChessEngine
{
public:
	ChessEngine( const gameConfig_t& cfg ) { Init( cfg ); }
	ChessEngine() {}

	~ChessEngine()
	{
		pieceNum = 0;
		for ( int i = 0; i < PieceCount; ++i )
		{
			delete s.pieces[ i ];
		}
	}

	void Init( const gameConfig_t& cfg )
	{
		pieceNum = 0;
		winner = teamCode_t::NONE;
		inCheck = teamCode_t::NONE;
		memset( s.pieces, 0, sizeof( Piece* ) * PieceCount );
		config = cfg;
		SetBoard( config );
		s.game = this;
		s.CountTeamPieces();
	}

	static Piece* CreatePiece( const pieceType_t pieceType, const teamCode_t teamCode );

	static inline teamCode_t GetOpposingTeam( const teamCode_t team ) { return ( team == teamCode_t::WHITE ) ? teamCode_t::BLACK : teamCode_t::WHITE; }

	resultCode_t Execute( const command_t& cmd )
	{
		const pieceHandle_t piece = FindPiece( cmd.team, cmd.pieceType, cmd.instance );
		if ( piece == NoPiece )
		{
			return resultCode_t::RESULT_GAME_INVALID_PIECE;
		}

		if ( GetWinner() != teamCode_t::NONE )
		{
			return resultCode_t::RESULT_GAME_COMPLETE;
		}

		if ( PerformMoveAction( piece, cmd.x, cmd.y ) == false )
		{
			return resultCode_t::RESULT_GAME_INVALID_MOVE;
		}
		return ( GetWinner() != teamCode_t::NONE ) ? resultCode_t::RESULT_GAME_COMPLETE : resultCode_t::RESULT_SUCCESS;
	}

	void GetTeamCaptures( const teamCode_t teamCode, pieceInfo_t capturedPieces[ TeamPieceCount ], int& captureCount ) const
	{
		memset( capturedPieces, 0, sizeof( capturedPieces[ 0 ] ) * TeamPieceCount );
		const int index = static_cast<int>( teamCode );
		if ( ( index >= 0 ) && ( index < TeamCount ) )
		{
			captureCount = s.teams[ index ].capturedCount;
			for ( int i = 0; i < s.teams[ index ].capturedCount; ++i )
			{
				capturedPieces[ i ] = GetInfo( s.teams[ index ].captured[ i ] );
			}
		}
	}

	void EnumerateActions( const pieceHandle_t pieceHdl, std::vector< moveAction_t >& actionList ) const
	{
		const Piece* piece = s.GetPiece( pieceHdl );
		if ( piece != nullptr )
		{
			const int actionCount = piece->GetActionCount();
			for ( int action = 0; action < actionCount; ++action )
			{
				moveAction_t path[ BoardSize ];
				const int pathLength = piece->GetActionPath( action, path );
				for ( int i = 0; i < pathLength; ++i )
				{
					actionList.push_back( path[ i ] );
				}
			}
		}
	}

	inline const pieceHandle_t FindPiece( const teamCode_t team, const pieceType_t type, const int instance ) const
	{
		return const_cast<ChessEngine*>( this )->FindPiece( team, type, instance );
	}

	pieceHandle_t		FindPiece( const teamCode_t team, const pieceType_t type, const int instance );
	pieceInfo_t			GetInfo( const pieceHandle_t pieceType ) const;
	pieceInfo_t			GetInfo( const int x, const int y ) const;
	bool				GetLocation( const pieceHandle_t pieceType, int& x, int& y ) const;
	void				SetEventCallback( callback_t callback ) { this->s.callback = callback; }
	inline teamCode_t	GetWinner() const { return winner; }
	inline int			GetPieceCount() const { return pieceNum; }
	bool				IsValidHandle( const pieceHandle_t handle ) const;

private:
	void				SetBoard( const gameConfig_t& cfg );
	void				EnterPieceInGame( Piece* piece, const int x, const int y );
	bool				PerformMoveAction( const pieceHandle_t pieceHdl, const int targetX, const int targetY );
private:
	ChessState			s;
	int					pieceNum;
	teamCode_t			winner;
	teamCode_t			inCheck;
	gameConfig_t		config;
};

// Backwards compatibility
using Chess = ChessEngine;


// ============================================================
// Command helpers
// ============================================================

static char GetPieceCode( const pieceType_t type )
{
	switch ( type )
	{
	case pieceType_t::PAWN:		return 'p';
	case pieceType_t::ROOK:		return 'r';
	case pieceType_t::KNIGHT:	return 'n';
	case pieceType_t::BISHOP:	return 'b';
	case pieceType_t::KING:		return 'k';
	case pieceType_t::QUEEN:	return 'q';
	}
	return '?';
}

static pieceType_t GetPieceType( const char code )
{
	switch ( tolower( code ) )
	{
	case 'p':	return pieceType_t::PAWN;
	case 'r':	return pieceType_t::ROOK;
	case 'n':	return pieceType_t::KNIGHT;
	case 'b':	return pieceType_t::BISHOP;
	case 'k':	return pieceType_t::KING;
	case 'q':	return pieceType_t::QUEEN;
	}
	return pieceType_t::NONE;
}

static int GetFileNum( const char file )
{
	switch ( tolower( file ) )
	{
	case 'a':	return 0;
	case 'b':	return 1;
	case 'c':	return 2;
	case 'd':	return 3;
	case 'e':	return 4;
	case 'f':	return 5;
	case 'g':	return 6;
	case 'h':	return 7;
	}
	return -1;
}

static char GetFile( const int fileNum )
{
	switch ( fileNum )
	{
	case 0:		return 'a';
	case 1:		return 'b';
	case 2:		return 'c';
	case 3:		return 'd';
	case 4:		return 'e';
	case 5:		return 'f';
	case 6:		return 'g';
	case 7:		return 'h';
	}
	return '?';
}

static int GetRankNum( const char rank )
{
	switch ( tolower( rank ) )
	{
	case '1':	return 7;
	case '2':	return 6;
	case '3':	return 5;
	case '4':	return 4;
	case '5':	return 3;
	case '6':	return 2;
	case '7':	return 1;
	case '8':	return 0;
	}
	return -1;
}

static char GetRank( const int rankNum )
{
	switch ( rankNum )
	{
	case 0:		return '8';
	case 1:		return '7';
	case 2:		return '6';
	case 3:		return '5';
	case 4:		return '4';
	case 5:		return '3';
	case 6:		return '2';
	case 7:		return '1';
	}
	return '?';
}

static resultCode_t TranslateActionCommand( const ChessEngine& board, const teamCode_t team, const std::string& commandString, command_t& outCmd )
{
	if ( commandString.size() != 4 )
	{
		return resultCode_t::RESULT_INPUT_INVALID_COMMAND;
	}

	outCmd.team = team;
	outCmd.pieceType = GetPieceType( commandString[ 0 ] );
	outCmd.instance = commandString[ 1 ] - '0';

	if ( board.FindPiece( outCmd.team, outCmd.pieceType, outCmd.instance ) == NoPiece )
	{
		return resultCode_t::RESULT_INPUT_INVALID_PIECE;
	}

	outCmd.x = GetFileNum( commandString[ 2 ] );
	if ( ( outCmd.x < 0 ) && ( outCmd.x >= BoardSize ) )
	{
		return resultCode_t::RESULT_INPUT_INVALID_FILE;
	}

	outCmd.y = GetRankNum( commandString[ 3 ] );
	if ( ( outCmd.y < 0 ) && ( outCmd.y >= BoardSize ) )
	{
		return resultCode_t::RESULT_INPUT_INVALID_RANK;
	}

	return resultCode_t::RESULT_SUCCESS;
}
