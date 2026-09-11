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
#include <set>


// ============================================================
// Constants
// ============================================================

typedef int8_t num_t;

static const num_t BoardSize = 8;
static const num_t TeamCount = 2;
static const num_t TeamPieceCount = 16;
static const num_t PieceCount = 32;
typedef num_t pieceHandle_t;
static const pieceHandle_t NoPiece = -1;
static const pieceHandle_t DummyPiece = static_cast< num_t >( ~0 );
static const pieceHandle_t OffBoard = -2;

#define USE_MOVE_CACHE_TEST 1

class MoveCache
{
public:
	// Quick move superset check
	// This is just a bitboard, used since a full bitboard conversion takes more time and is harder to debug
	// A move can potentially go +/-7 in any direction so you need a hypothetical 15x15 board
	// Traditional bitboards uses a 64 bit uint where each bit is a square, this requires a larger board however
	// A quad of boards covers the 15x15 space and is stored as a flattened 2D array
	// High bits access a quad, lower bits the squares within a quad

	uint64_t bits[ 4 ] = {};

	void Set( int32_t dx, int32_t dy )
	{
		const int32_t idx = ( dy + 7 ) * 15 + ( dx + 7 );
		bits[ idx >> 6 ] |= ( 1ull << ( idx & 63 ) );
	}

	bool Test( int32_t dx, int32_t dy ) const
	{
		const int32_t idx = ( dy + 7 ) * 15 + ( dx + 7 );
		return ( bits[ idx >> 6 ] >> ( idx & 63 ) ) & 1;
	}
};

class ChessEngine;


// ============================================================
// Enums
// ============================================================

enum resultCode_t
{
	RESULT_SUCCESS = 0,

	RESULT_INPUT_INVALID_COMMAND = ( 1 << 0 ),
	RESULT_INPUT_INVALID_PIECE = ( 1 << 1 ),
	RESULT_INPUT_INVALID_FILE = ( 1 << 2 ),
	RESULT_INPUT_INVALID_RANK = ( 1 << 3 ),
	RESULT_INPUT_INVALID_MOVE = ( 1 << 4 ),
	RESULT_GAME_INVALID_PIECE = ( 1 << 5 ),
	RESULT_GAME_INVALID_MOVE = ( 1 << 6 ),
	RESULT_GAME_ERROR_MASK = ( RESULT_INPUT_INVALID_COMMAND | RESULT_INPUT_INVALID_PIECE | \
											RESULT_INPUT_INVALID_FILE | RESULT_INPUT_INVALID_RANK | \
											RESULT_GAME_INVALID_PIECE | RESULT_GAME_INVALID_MOVE ),

	RESULT_GAME_COMPLETE_WHITE_WINS = ( 1 << 7 ),
	RESULT_GAME_COMPLETE_BLACK_WINS = ( 1 << 8 ),
	RESULT_GAME_COMPLETE_STALEMATE = ( 1 << 9 ),
	RESULT_GAME_COMPLETE_WHITE_RESIGNS = ( 1 << 10 ),
	RESULT_GAME_COMPLETE_BLACK_RESIGNS = ( 1 << 11 ),
	RESULT_GAME_COMPLETE = ( RESULT_GAME_COMPLETE_WHITE_WINS | RESULT_GAME_COMPLETE_BLACK_WINS | \
											RESULT_GAME_COMPLETE_WHITE_RESIGNS | RESULT_GAME_COMPLETE_BLACK_RESIGNS | \
											RESULT_GAME_COMPLETE_STALEMATE ),
};


struct resultMessage_t
{
	resultCode_t	code;
	const char* msg;
};


constexpr static uint32_t ResultMessageCount = 12;
static resultMessage_t ResultMsgs[ ResultMessageCount ] =
{
	{ RESULT_INPUT_INVALID_COMMAND,			"Invalid command"			},
	{ RESULT_INPUT_INVALID_PIECE,			"Invalid piece"				},
	{ RESULT_INPUT_INVALID_FILE,			"File out of range"			},
	{ RESULT_INPUT_INVALID_RANK,			"Rank out of range"			},
	{ RESULT_INPUT_INVALID_MOVE,			"Invalid move command",		},
	{ RESULT_GAME_INVALID_PIECE,			"Invalid piece selected",	},
	{ RESULT_GAME_INVALID_MOVE,				"Invalid move action",		},
	{ RESULT_GAME_COMPLETE_WHITE_WINS,		"White Wins",				},
	{ RESULT_GAME_COMPLETE_BLACK_WINS,		"Black Wins",				},
	{ RESULT_GAME_COMPLETE_WHITE_RESIGNS,	"White Resigns",			},
	{ RESULT_GAME_COMPLETE_BLACK_RESIGNS,	"Black Resigns",			},
	{ RESULT_GAME_COMPLETE_STALEMATE,		"Stalemate",				},
};


inline const char* GetErrorMsg( const resultCode_t code )
{
	for( int32_t i = 0; i < ResultMessageCount; ++i ) {
		if( ResultMsgs[ i ].code == code ) {
			return ResultMsgs[ i ].msg;
		}
	}
	return "Unknown Error";
}


enum class pieceType_t : int32_t
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


enum class teamCode_t : int32_t
{
	NONE = -1,
	WHITE = 0,
	BLACK = 1,
	COUNT
};

// T - Top, L - Left, R - Right, B - Bottom
// Pawns are the only piece where moves are relative
enum class moveType_t : int32_t
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

	PAWN_ACTIONS = ( PAWN_KILL_R - PAWN_T ) + 1,
	KNIGHT_ACTIONS = ( KNIGHT_B1L2 - KNIGHT_T1L2 ) + 1,
	ROOK_ACTIONS = ( ROOK_B - ROOK_L ) + 1,
	BISHOP_ACTIONS = ( BISHOP_BL - BISHOP_TL ) + 1,
	KING_ACTIONS = ( KING_CASTLE_R - KING_TL ) + 1,
	QUEEN_ACTIONS = ( QUEEN_L - QUEEN_TL ) + 1,
	MOVE_COUNT = ( PAWN_ACTIONS + KNIGHT_ACTIONS + ROOK_ACTIONS + BISHOP_ACTIONS + KING_ACTIONS + QUEEN_ACTIONS ),
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


struct position_t
{
	num_t	x;
	num_t	y;
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

	moveAction_t( const int32_t x, int32_t const y, const moveType_t type, const int32_t maxSteps ) :
		x( x ), y( y ), type( type ), maxSteps( maxSteps ) {}

	num_t			x;
	num_t			y;
	num_t			maxSteps;	// How many times can this action be repeated?
	moveType_t		type;		// For specialized logic and debugging
};


struct command_t
{
	int32_t			x;
	int32_t			y;
	pieceType_t		pieceType;
	int32_t			instance;
	teamCode_t		team;
};


struct team_t
{
	team_t()
	{
		for( int32_t i = 0; i < TeamPieceCount; ++i ) {
			pieces[ i ] = NoPiece;
			captured[ i ] = NoPiece;
		}
		for( int32_t i = 0; i < ( int32_t )pieceType_t::COUNT; ++i ) {
			typeCounts[ i ] = 0;
			captureTypeCounts[ i ] = 0;
		}
		livingCount = 0;
		capturedCount = 0;
	}

	pieceHandle_t	pieces[ TeamPieceCount ];
	pieceHandle_t	captured[ TeamPieceCount ];
	num_t			livingCount;
	num_t			capturedCount;
	num_t			typeCounts[ ( int32_t )pieceType_t::COUNT ];
	num_t			captureTypeCounts[ ( int32_t )pieceType_t::COUNT ];
};

struct pieceInfo_t
{
	teamCode_t		team;		// White, Black, etc
	pieceType_t		pieceType;	// Pawn, Knight, etc
	int32_t			instance;	// Pawn0, Pawn1, etc
	bool			onBoard;	// If false: Off-board location (-1,-1), captured piece, preparing to play (e.g. promotion)
	bool			isPiece;	// Quick check for a player piece
};

struct gameConfig_t
{
	gameConfig_t() {};

	gameConfig_t( const gameConfig_t& config )
	{
		for( int32_t i = 0; i < BoardSize; ++i ) {
			for( int32_t j = 0; j < BoardSize; ++j ) {
				board[ i ][ j ] = config.board[ i ][ j ];
			}
		}
	}

	gameConfig_t& operator=( const gameConfig_t& config )
	{
		if( this == &config ) {
			return *this;
		}
		for( int32_t i = 0; i < BoardSize; ++i ) {
			for( int32_t j = 0; j < BoardSize; ++j ) {
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
std::string SquareToString( const ChessEngine& chessEngine, const int32_t x, const int32_t y );
std::string TeamCaptureString( const ChessEngine& chessEngine, const teamCode_t team );
std::string BoardToString( const ChessEngine& chessEngine, const bool printCaptures );
void LoadConfig( const std::string& fileName, gameConfig_t& config );
void LoadHistory( const std::string& fileName, std::vector< std::string >& commands );

inline static void AutoPromoteQueen( callbackEvent_t& event )
{
	event.promotionType = pieceType_t::QUEEN;
}


// ============================================================
// Move action tables
// ============================================================


static const moveAction_t PawnActions[ ( int32_t )moveType_t::PAWN_ACTIONS ] =
{
	moveAction_t( 0, 1, moveType_t::PAWN_T, 1 ),
	moveAction_t( 0, 1, moveType_t::PAWN_T2X, 2 ),
	moveAction_t( -1, 1, moveType_t::PAWN_KILL_L, 1 ),
	moveAction_t( 1, 1, moveType_t::PAWN_KILL_R, 1 )
};


static const moveAction_t RookActions[ ( int32_t )moveType_t::ROOK_ACTIONS ] =
{
	moveAction_t( -1, 0, moveType_t::ROOK_L, BoardSize - 1 ),
	moveAction_t( 1, 0, moveType_t::ROOK_R, BoardSize - 1 ),
	moveAction_t( 0, 1, moveType_t::ROOK_T, BoardSize - 1 ),
	moveAction_t( 0, -1, moveType_t::ROOK_B, BoardSize - 1 ),
};


static const moveAction_t KnightActions[ ( int32_t )moveType_t::KNIGHT_ACTIONS ] =
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


static const moveAction_t BishopActions[ ( int32_t )moveType_t::BISHOP_ACTIONS ] =
{
	moveAction_t( -1, -1, moveType_t::BISHOP_TL, BoardSize - 1 ),
	moveAction_t( 1, -1, moveType_t::BISHOP_TR, BoardSize - 1 ),
	moveAction_t( 1, 1, moveType_t::BISHOP_BR, BoardSize - 1 ),
	moveAction_t( -1, 1, moveType_t::BISHOP_BL, BoardSize - 1 )
};


static const moveAction_t KingActions[ ( int32_t )moveType_t::KING_ACTIONS ] =
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


static const moveAction_t QueenActions[ ( int32_t )moveType_t::QUEEN_ACTIONS ] =
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


extern MoveCache PawnMoveSuperset;
extern MoveCache RookMoveSuperset;
extern MoveCache KnightMoveSuperset;
extern MoveCache BishopMoveSuperset;
extern MoveCache KingMoveSuperset;
extern MoveCache QueenMoveSuperset;


// ============================================================
// Piece classes
// ============================================================

class ChessState;

class Piece
{
protected:
	static const int32_t MaxActions = 10;

	Piece()
	{
		team = teamCode_t::NONE;
		type = pieceType_t::NONE;

		m_prevX = -1;
		m_prevY = -1;
		m_x = -1;
		m_y = -1;

		m_numActions = 0;
		m_teamDirection = 1;
		m_moveCount = 0;

		m_instance = 0;
		m_handle = NoPiece;

		m_state = nullptr;
		m_actions = nullptr;
		m_moveSuperset = nullptr;

		m_promoted = false;
	}

	bool			IsValidAction( const int32_t actionNum ) const;
public:

	void			CalculateStep( const int32_t actionNum, num_t& actionX, num_t& actionY ) const;					// Move one square along an action path (e.g. rook, bishop, queen, paths can be a single step)
	num_t			GetStepCount( const int32_t actionNum, const num_t targetX, const num_t targetY ) const;		// How many squares are traveled for this action?
	num_t			GetActionPath( const int32_t actionNum, moveAction_t path[ BoardSize ] ) const;					// Get all squares in this action's path
	void			FillMoveCache();
	int32_t			ComputeAllMoveActions( position_t* moves ) const;												// Generate all moves for a piece
	int32_t			ComputeActionPath( const int32_t actionNum, position_t* path ) const;							// Generate path for an action
	bool			InActionPath( const int32_t actionNum, const num_t targetX, const num_t targetY ) const;		// This action can reach this location
	void			Move( const moveType_t moveType, const num_t targetX, const num_t targetY );					// Performs a game move, rules run
	void			PlaceAt( const num_t targetX, const num_t targetY );											// Places a piece at a location, rules not runn. Temp moves, castling, etc

	bool			IsPawnMoveValid( const int32_t actionNum, const num_t targetX, const num_t targetY ) const;		// Specialized checks for pawns (double-move, capture, enpassants)
	bool			IsKingMoveValid( const int32_t actionNum, const num_t targetX, const num_t targetY ) const;		// Specialized checks for kings (castling)

	void			TempPlacement( const num_t targetX, const num_t targetY );										// Place the piece offboard, outside rules engine. Assists other rule checks
	void			ReturnPlacement();																				// Return the piece offboard, outside rules engine. Assists other rule checks

	bool			CanPromote() const;																				// Pawn promotion
	void			Promote();																						// Pawn promotion

	inline num_t	X() const { return m_x; }
	inline num_t	Y() const { return m_y; }
	inline num_t	GetInstanceNumber() const { return m_instance; }

	bool			HasMoved() const { return ( m_moveCount > 0 ); }												// Has this piece been moved in this game? (for castling, book-keeping)
	int32_t			GetActionCount() const { return m_numActions; }													// How many unique move actions can a piece perform?
	bool			OnBoard() const { return ( m_state != nullptr ); }												// Is the piece in play? (e.g. not captured)
	inline int32_t	GetTeamDirection() const { return m_teamDirection; }											// Used for pawn movement

	void RemoveFromPlay()
	{
		PlaceAt( -1, -1 );
		m_state = nullptr;
	}

	int32_t GetActionNum( const moveType_t moveType ) const
	{
		const moveAction_t* actions = GetActions();
		const int32_t number = ( static_cast< int32_t >( moveType ) - static_cast< int32_t >( actions[ 0 ].type ) );

		assert( actions[ number ].type == moveType );

		return number;
	}

	const moveAction_t& GetAction( const int32_t actionNum ) const
	{
		return m_actions[ actionNum ];
	}

	const moveAction_t* GetActions() const { return m_actions; }
	const MoveCache& GetMoveCache() const { return *m_moveSuperset; }

private:

	inline void SetInstanceNumber( const num_t instance )
	{
		m_instance = instance;
	}

	void BindBoard( ChessState* state, const pieceHandle_t handle )
	{
		this->m_state = state;
		this->m_handle = handle;


	}

public:
	teamCode_t			team;
	pieceType_t			type;

protected:
	num_t				m_prevX;		// State. For temp actions
	num_t				m_prevY;		// State. "
	num_t				m_x;			// State. For convenience, board state in ChessState is authoritative (i.e. can set this from the board)
	num_t				m_y;			// State. "
	num_t				m_instance;
	num_t				m_moveCount;
	num_t				m_numActions;
	num_t				m_teamDirection;
	bool				m_promoted;
	pieceHandle_t		m_handle;

	const moveAction_t* m_actions;
	const MoveCache* m_moveSuperset;
	ChessState* m_state;

	friend class ChessEngine;
	friend class ChessState;
};


// ============================================================
// ChessState
// ============================================================

class ChessState
{
public:
	ChessState() {}
	~ChessState() {}

	moveType_t			IsLegalMove( const Piece* piece, const num_t targetX, const num_t targetY ) const;				// Core function, runs all rule evaluation
	inline bool			OnBoard( const num_t x, const num_t y ) const;													// Bounds check for path checking
	inline const Piece* GetPiece( const pieceHandle_t handle ) const;
	inline Piece* GetPiece( const pieceHandle_t handle );
	const Piece* GetPiece( const num_t x, const num_t y ) const;
	Piece* GetPiece( const num_t x, const num_t y );
	void				SetHandle( const pieceHandle_t pieceHdl, const num_t x, const num_t y );
	pieceHandle_t		GetHandle( const num_t x, const num_t y ) const;

	void				CapturePiece( const teamCode_t attacker, Piece* targetPiece );
	bool				IsBlocked( const teamCode_t team, const num_t x, const num_t y ) const;
	bool				IsKingCaptured( const teamCode_t checkedTeamCode ) const;
	bool				IsChecked( const teamCode_t checkedTeamCode ) const;
	bool				IsCheckMate( const Piece* attacker, const moveType_t& moveType, const teamCode_t checkedTeamCode ) const;
	bool				IsStalemate( const teamCode_t teamCode ) const;
	bool				IsOpenToAttack( const Piece* targetPiece ) const;
	bool				IsOpenToAttackAt( const Piece* targetPiece, const num_t targetX, const num_t targetY ) const;
	Piece* GetEnpassant( const num_t targetX, const num_t targetY );
	inline void			SetEnpassant( const pieceHandle_t handle ) { m_enpassantPawn = handle; }						// Saves enpassant pawn for next turn checks

	void				PromotionCallback( const teamCode_t teamCode, callbackEvent_t& event );

	// Unwind speculative search actions
	void				ReverseCapturePiece( const teamCode_t attacker, Piece* targetPiece );

	// Deep copy for validation
	void				CopyFrom( const ChessState& src );
	bool				Compare( const ChessState& other ) const;

private:
	pieceHandle_t		m_enpassantPawn;
	team_t				m_teams[ TeamCount ];
	pieceHandle_t		m_grid[ BoardSize ][ BoardSize ]; // (0,0) is top left, mutable for quick tests (const-functions should always reverse)
	ChessEngine* m_game;

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
		m_pieceNum = 0;
		for( int32_t i = 0; i < PieceCount; ++i ) {
			delete m_pieces[ i ];
		}
	}

	void Init( const gameConfig_t& cfg )
	{
		m_pieceNum = 0;
		m_winner = teamCode_t::NONE;

		memset( m_pieces, 0, sizeof( Piece* ) * PieceCount );
		m_config = cfg;
		SetBoard( m_config );
		m_state.m_game = this;

		SetPromotionCallback( teamCode_t::WHITE, &AutoPromoteQueen );
		SetPromotionCallback( teamCode_t::BLACK, &AutoPromoteQueen );

		const bool whiteChecked = m_state.IsChecked( teamCode_t::WHITE );
		const bool blackChecked = m_state.IsChecked( teamCode_t::BLACK );

		assert( ( whiteChecked && blackChecked ) == false ); // Should be impossible, but maybe ok in test-cases?

		if( whiteChecked ) {
			m_checkedTeam = teamCode_t::WHITE;
		}

		if( blackChecked ) {
			m_checkedTeam = teamCode_t::BLACK;
		}
	}

	static Piece* CreatePiece( const pieceType_t pieceType, const teamCode_t teamCode );
	static void DestroyPiece( Piece*& piece );

	static inline teamCode_t GetOpposingTeam( const teamCode_t team ) { return ( team == teamCode_t::WHITE ) ? teamCode_t::BLACK : teamCode_t::WHITE; }

	resultCode_t Execute( const command_t& cmd )
	{
		const pieceHandle_t piece = FindPiece( cmd.team, cmd.pieceType, cmd.instance );
		if( piece == NoPiece ) {
			return resultCode_t::RESULT_GAME_INVALID_PIECE;
		}

		if( GetWinner() != teamCode_t::NONE ) {
			return GetGameResult();
		}

		if( PerformMoveAction( piece, cmd.x, cmd.y ) == false ) {
			return resultCode_t::RESULT_GAME_INVALID_MOVE;
		}

		return ( GetWinner() != teamCode_t::NONE || IsStalemate() ) ? GetGameResult() : resultCode_t::RESULT_SUCCESS;
	}

	resultCode_t GetGameResult() const
	{
		if( m_stalemate ) {
			return resultCode_t::RESULT_GAME_COMPLETE_STALEMATE;
		}
		if( m_winner == teamCode_t::WHITE ) {
			return resultCode_t::RESULT_GAME_COMPLETE_WHITE_WINS;
		}
		if( m_winner == teamCode_t::BLACK ) {
			return resultCode_t::RESULT_GAME_COMPLETE_BLACK_WINS;
		}
		return resultCode_t::RESULT_SUCCESS;
	}


	void GetTeamCaptures( const teamCode_t teamCode, pieceInfo_t capturedPieces[ TeamPieceCount ], int32_t& captureCount ) const
	{
		memset( capturedPieces, 0, sizeof( capturedPieces[ 0 ] ) * TeamPieceCount );
		const int32_t index = static_cast< int32_t >( teamCode );
		if( ( index >= 0 ) && ( index < TeamCount ) ) {
			captureCount = m_state.m_teams[ index ].capturedCount;
			for( int32_t i = 0; i < m_state.m_teams[ index ].capturedCount; ++i ) {
				capturedPieces[ i ] = GetInfo( m_state.m_teams[ index ].captured[ i ] );
			}
		}
	}

	void EnumerateActions( const pieceHandle_t pieceHdl, std::vector< moveAction_t >& actionList ) const
	{
		const Piece* piece = m_state.GetPiece( pieceHdl );
		if( piece != nullptr ) {
			const int32_t actionCount = piece->GetActionCount();
			for( int32_t action = 0; action < actionCount; ++action ) {
				moveAction_t path[ BoardSize ];
				const int32_t pathLength = piece->GetActionPath( action, path );
				for( int32_t i = 0; i < pathLength; ++i ) {
					actionList.push_back( path[ i ] );
				}
			}
		}
	}

	inline const pieceHandle_t FindPiece( const teamCode_t team, const pieceType_t type, const num_t instance ) const
	{
		return const_cast< ChessEngine* >( this )->FindPiece( team, type, instance );
	}

	pieceHandle_t		FindPiece( const teamCode_t team, const pieceType_t type, const num_t instance );				// Find piece given identifying info (e.g. White-King)
	pieceInfo_t			GetInfo( const pieceHandle_t pieceType ) const;													// Query info about a given piece
	pieceInfo_t			GetInfo( const num_t x, const num_t y ) const;													// Query info for the selected square (team, piece type, etc)
	bool				GetLocation( const pieceHandle_t pieceType, num_t& x, num_t& y ) const;							// Query the location given a piece
	inline bool			IsStalemate() const { return m_stalemate; }														// Is stalemate
	inline teamCode_t	GetCurrentPlayer() { return m_currentTurn; }													// Player for current turn
	inline teamCode_t	GetWinner() const { return m_winner; }															// Winning team or none
	inline teamCode_t	GetCheckedTeam() const { return m_checkedTeam; }												// Checked team or none
	inline num_t		GetPieceCount() const { return m_pieceNum; }													// Piece count given this game config
	bool				IsValidHandle( const pieceHandle_t handle ) const;												// Is this piece handle valid? (Likely, yes)

	inline void			SetPromotionCallback( const teamCode_t team, callback_t callback ) { m_promotionCallback[ ( int32_t )team ] = callback; }

private:
	void				SetBoard( const gameConfig_t& cfg );															// Sets the board up for a given play-state (e.g. default starting set-up)
	void				EnterPieceInGame( Piece* piece, const num_t x, const num_t y );									// Registers the piece and places it on the board
	bool				PerformMoveAction( const pieceHandle_t pieceHdl, const num_t targetX, const num_t targetY );	// Performs a game move
	void				CalculateGameState( const moveType_t& moveType, const pieceHandle_t movedPieceHdl );			// Checkmate, Check, Stalemate
	int32_t				Search( int32_t depth );																		// A.I. search

	inline void			PromotionCallback( const teamCode_t teamCode, callbackEvent_t& event )							// User needs to make their pick of piece, A.I. can run a heuristic
	{
		if( m_promotionCallback[ ( int32_t )teamCode ] != nullptr ) {
			( *m_promotionCallback[ ( int32_t )teamCode ] )( event );
		};
	}

private:
	ChessState			m_state;
	Piece* m_pieces[ PieceCount ];
	callback_t			m_promotionCallback[ TeamCount ];
	num_t				m_pieceNum;
	int32_t				m_turnCount;
	teamCode_t			m_currentTurn;
	teamCode_t			m_winner;
	teamCode_t			m_checkedTeam;
	bool				m_stalemate;
	gameConfig_t		m_config;

	friend class ChessState;
};

// Backwards compatibility
using Chess = ChessEngine;


// ============================================================
// Command helpers
// ============================================================

static char GetPieceCode( const pieceType_t type )
{
	switch( type ) {
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
	switch( tolower( code ) ) {
	case 'p':	return pieceType_t::PAWN;
	case 'r':	return pieceType_t::ROOK;
	case 'n':	return pieceType_t::KNIGHT;
	case 'b':	return pieceType_t::BISHOP;
	case 'k':	return pieceType_t::KING;
	case 'q':	return pieceType_t::QUEEN;
	}
	return pieceType_t::NONE;
}

static int32_t GetFileNum( const char file )
{
	switch( tolower( file ) ) {
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

static char GetFile( const int32_t fileNum )
{
	switch( fileNum ) {
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

static int32_t GetRankNum( const char rank )
{
	switch( tolower( rank ) ) {
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

static char GetRank( const int32_t rankNum )
{
	switch( rankNum ) {
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
	if( commandString == "Resign" || commandString == "resign" ) {
		return ( team == teamCode_t::WHITE ) ? resultCode_t::RESULT_GAME_COMPLETE_WHITE_RESIGNS : resultCode_t::RESULT_GAME_COMPLETE_BLACK_RESIGNS;
	}

	if( commandString.size() != 4 ) {
		return resultCode_t::RESULT_INPUT_INVALID_COMMAND;
	}

	outCmd.team = team;
	outCmd.pieceType = GetPieceType( commandString[ 0 ] );
	outCmd.instance = commandString[ 1 ] - '0';

	if( board.FindPiece( outCmd.team, outCmd.pieceType, outCmd.instance ) == NoPiece ) {
		return resultCode_t::RESULT_INPUT_INVALID_PIECE;
	}

	outCmd.x = GetFileNum( commandString[ 2 ] );
	if( ( outCmd.x < 0 ) || ( outCmd.x >= BoardSize ) ) {
		return resultCode_t::RESULT_INPUT_INVALID_FILE;
	}

	outCmd.y = GetRankNum( commandString[ 3 ] );
	if( ( outCmd.y < 0 ) || ( outCmd.y >= BoardSize ) ) {
		return resultCode_t::RESULT_INPUT_INVALID_RANK;
	}

	return resultCode_t::RESULT_SUCCESS;
}
