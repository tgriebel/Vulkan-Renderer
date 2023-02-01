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

#include "chessScene.h"

extern imguiControls_t	gImguiControls;
extern Window			gWindow;

struct pieceMappingInfo_t {
	const char* name;
};


static std::string pieceNames[ 8 ] =
{
	"white_pawn_0",
	"white_pawn_1",
	"white_pawn_2",
	"white_pawn_3",
	"white_pawn_4",
	"white_pawn_5",
	"white_pawn_6",
	"white_pawn_7",
};


static std::string GetModelName( const pieceType_t type )
{
	switch ( type ) {
	case pieceType_t::PAWN:
		return "pawn";
		break;
	case pieceType_t::ROOK:
		return "rook";
		break;
	case pieceType_t::KNIGHT:
		return "knight";
		break;
	case pieceType_t::BISHOP:
		return "bishop";
		break;
	case pieceType_t::QUEEN:
		return "queen";
		break;
	case pieceType_t::KING:
		return "king";
		break;
	}
	return "<none>";
}


static std::string GetName( pieceInfo_t& pieceInfo )
{
	std::string name;
	if ( pieceInfo.team == teamCode_t::WHITE ) {
		name += "white";
	}
	else {
		name += "black";
	}
	name += "_" + GetModelName( pieceInfo.piece );
	name += "_" + std::to_string( pieceInfo.instance );
	return name;
}


static vec3f GetSquareCenterForLocation( const char file, const char rank )
{
	const vec3f whiteCorner = vec3f( -7.0f, -7.0f, 0.0f );
	const int x = GetFileNum( file );
	const int y = GetRankNum( rank );
	return whiteCorner + vec3f( 2.0f * x, 2.0f * ( 7 - y ), 0.0f );
}


void ChessScene::Init()
{
	const int piecesNum = 16;

	gameConfig_t cfg;
	LoadConfig( "scenes/chessCfg/default_board.txt", cfg );
	chessEngine.Init( cfg );
	//board.SetEventCallback( &ProcessEvent );

	{
		Entity* ent = new Entity();
		CreateEntityBounds( gAssets.modelLib.RetrieveHdl( "_skybox" ), *ent );
		ent->name = "_skybox";
		entities.push_back( ent );
	}

	gAssets.modelLib.Find( "plane" )->Get().surfs[ 0 ].materialHdl = gAssets.materialLib.RetrieveHdl( "GlowSquare" );

	for ( int i = 0; i < 8; ++i )
	{
		for ( int j = 0; j < 8; ++j )
		{
			PieceEntity* squareEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			CreateEntityBounds( gAssets.modelLib.RetrieveHdl( "plane" ), *squareEnt );
			squareEnt->SetOrigin( GetSquareCenterForLocation( squareEnt->file, squareEnt->rank ) + vec3f( 0.0f, 0.0f, 0.01f ) );
			squareEnt->SetFlag( ENT_FLAG_SELECTABLE );
			squareEnt->handle = -1;
			std::string name = "plane_";
			name += std::to_string( i ) + "_" + std::to_string( j );

			squareEnt->name = name.c_str();
			glowEntities.push_back( static_cast<uint32_t>( entities.size() ) );
			entities.push_back( squareEnt );

			pieceInfo_t pieceInfo = chessEngine.GetInfo( j, i );
			if ( pieceInfo.onBoard == false ) {
				continue;
			}
			PieceEntity* pieceEnt = new PieceEntity( GetFile( j ), GetRank( i ) );
			CreateEntityBounds( gAssets.modelLib.RetrieveHdl( GetModelName( pieceInfo.piece ).c_str() ), *pieceEnt );
			pieceEnt->handle = chessEngine.FindPiece( pieceInfo.team, pieceInfo.piece, pieceInfo.instance );
			pieceEnt->SetFlag( ENT_FLAG_SELECTABLE );
			if ( pieceInfo.team == teamCode_t::WHITE ) {
				pieceEnt->materialHdl = gAssets.materialLib.RetrieveHdl( "White.001" );
			}
			else {
				pieceEnt->SetRotation( vec3f( 0.0f, 0.0f, 180.0f ) );
				pieceEnt->materialHdl = gAssets.materialLib.RetrieveHdl( "Chess_Black.001" );
			}
			pieceEnt->name = GetName( pieceInfo ).c_str();
			pieceEntities.push_back( static_cast<uint32_t>( entities.size() ) );
			entities.push_back( pieceEnt );
		}
	}

	const hdl_t diamondHdl = gAssets.modelLib.RetrieveHdl( "diamond" );
	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* ent = new Entity();
		CreateEntityBounds( diamondHdl, *ent );
		ent->materialHdl = gAssets.materialLib.RetrieveHdl( "DEBUG_WIRE" );
		ent->SetFlag( ENT_FLAG_WIREFRAME );
		ent->name = ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str();
		entities.push_back( ent );
	}
}

void ChessScene::Update( const float dt )
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

	lights[ 0 ].lightPos = vec4f( 5.0f * cos( time ), 5.0f * sin( time ), 8.0f, 0.0f );

	const mouse_t& mouse = gWindow.input.GetMouse();
	if ( ( mouse.centered == false ) && mouse.leftDown )
	{
		Ray ray = camera.GetViewRay( vec2f( 0.5f * mouse.x + 0.5f, 0.5f * mouse.y + 0.5f ) );
		selectedEntity = GetTracedEntity( ray );
	}

	if ( selectedEntity != nullptr )
	{
		PieceEntity* selectedPiece = reinterpret_cast<PieceEntity*>( selectedEntity );

		int selectedActionIx = -1;
		for ( int actionIx = 0; actionIx < actions.size(); ++actionIx ) {
			const moveAction_t& action = actions[ actionIx ];
			if ( ( action.y == GetRankNum( selectedPiece->rank ) ) && ( action.x == GetFileNum( selectedPiece->file ) ) ) {
				selectedActionIx = actionIx;
				break;
			}
		}

		if ( ( selectedActionIx > 0 ) && ( movePieceId != nullptr ) )
		{
			PieceEntity* movedPiece = reinterpret_cast<PieceEntity*>( movePieceId );
			const pieceInfo_t movedPieceInfo = chessEngine.GetInfo( GetFileNum( movedPiece->file ), GetRankNum( movedPiece->rank ) );
			const moveAction_t& action = actions[ selectedActionIx ];

			command_t cmd;
			cmd.instance = movedPieceInfo.instance;
			cmd.team = movedPieceInfo.team;
			cmd.pieceType = movedPieceInfo.piece;
			cmd.x = action.x;
			cmd.y = action.y;

			const resultCode_t result = chessEngine.Execute( cmd );
			actions.resize( 0 );
		}
		else if ( selectedPiece->handle >= 0 )
		{
			actions.resize( 0 );
			chessEngine.EnumerateActions( selectedPiece->handle, actions );
			movePieceId = selectedEntity;
		}
		else
		{
			selectedActionIx = -1;
			selectedEntity = nullptr;
			movePieceId = nullptr;
		}
	}

	for ( uint32_t entityIx = 0; entityIx < static_cast<uint32_t>( pieceEntities.size() ); ++entityIx ) {
		const uint32_t pieceIx = pieceEntities[ entityIx ];
		PieceEntity* ent = reinterpret_cast<PieceEntity*>( entities[ pieceIx ] );
		if ( ent == selectedEntity ) {
			ent->outline = true;
		}
		else {
			ent->outline = false;
		}
		const pieceInfo_t info = chessEngine.GetInfo( ent->handle );
		int x, y;
		if ( chessEngine.GetLocation( ent->handle, x, y ) ) {
			ent->SetOrigin( GetSquareCenterForLocation( GetFile( x ), GetRank( y ) ) );
		}
	}

	for ( uint32_t entityIx = 0; entityIx < static_cast<uint32_t>( glowEntities.size() ); ++entityIx ) {
		const uint32_t glowIx = glowEntities[ entityIx ];
		PieceEntity* ent = reinterpret_cast<PieceEntity*>( FindEntity( glowIx ) );
		bool validTile = false;
		for ( int actionIx = 0; actionIx < actions.size(); ++actionIx ) {
			const moveAction_t& action = actions[ actionIx ];
			if ( ( action.y == GetRankNum( ent->rank ) ) && ( action.x == GetFileNum( ent->file ) ) ) {
				validTile = true;
				break;
			}
		}
		if ( validTile ) {
			ent->ClearFlag( ENT_FLAG_NO_DRAW );
		}
		else {
			ent->SetFlag( ENT_FLAG_NO_DRAW );
		}
	}

	Material& glowMat = gAssets.materialLib.Find( "GlowSquare" )->Get();
	glowMat.Kd = rgbTuplef_t( 0.1f, 0.1f, 1.0f );
	glowMat.Tr = 0.5f * cos( 3.0f * time ) + 0.5f;
	glowMat.dirty = true;

	for ( int i = 0; i < MaxLights; ++i )
	{
		Entity* debugLight = FindEntity( ( "light" + std::string( { (char)( (int)'0' + i ) } ) + "_dbg" ).c_str() );
		if ( debugLight == nullptr ) {
			continue;
		}
		vec3f origin = vec3f( lights[ i ].lightPos[ 0 ], lights[ i ].lightPos[ 1 ], lights[ i ].lightPos[ 2 ] );
		debugLight->SetOrigin( origin );
		debugLight->SetScale( vec3f( 0.25f, 0.25f, 0.25f ) );
	}
}