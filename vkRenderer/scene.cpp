#include "common.h"
#include "scene.h"
#include "util.h"

extern Scene scene;

void MakeBeachScene()
{
	scene.models.resize( 6 );
	const int skyBoxId = 0;
	const int terrainId = 1;
	const int palmModelId = 2;
	const int waterId = 3;
	const int toneQuadId = 4;
	const int image2dId = 5;

	CreateSkyBoxSurf( scene.models[ skyBoxId ] );
	CreateTerrainSurface( scene.models[ terrainId ] );
	CreateStaticModel( "sphere.obj", "PALM", scene.models[ palmModelId ] );
	CreateWaterSurface( scene.models[ waterId ] );
	CreateQuadSurface2D( "TONEMAP", scene.models[ toneQuadId ], glm::vec2( 1.0f, 1.0f ), glm::vec2( 2.0f ) );
	CreateQuadSurface2D( "IMAGE2D", scene.models[ image2dId ], glm::vec2( 1.0f, 1.0f ), glm::vec2( 1.0f * ( 9.0 / 16.0f ), 1.0f ) );

	const int palmTreesNum = 300;

	int entId = 0;
	scene.entities.resize( 5 + palmTreesNum );
	scene.CreateEntity( 0, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 1, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 3, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 4, scene.entities[ entId ] );
	++entId;
	scene.CreateEntity( 5, scene.entities[ entId ] );
	++entId;

	for ( int i = 0; i < palmTreesNum; ++i )
	{
		scene.CreateEntity( palmModelId, scene.entities[ entId ] );
		++entId;
	}

	// Terrain
	scene.entities[ 1 ].matrix = glm::identity<glm::mat4>();

	// Model
	const float scale = 0.0025f;
	{
		for ( int i = 5; i < ( 5 + palmTreesNum ); ++i )
		{
			glm::vec2 randPoint;
			RandPlanePoint( randPoint );
			randPoint = 10.0f * ( randPoint - 0.5f );
			scene.entities[ i ].matrix = scale * glm::identity<glm::mat4>();
			scene.entities[ i ].matrix[ 3 ][ 0 ] = randPoint.x;
			scene.entities[ i ].matrix[ 3 ][ 1 ] = randPoint.y;
			scene.entities[ i ].matrix[ 3 ][ 2 ] = 0.0f;
			scene.entities[ i ].matrix[ 3 ][ 3 ] = 1.0f;
		}
	}
}