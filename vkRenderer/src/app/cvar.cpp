#include "cvar.h"

static std::vector<CVar*> cvarMap;

CVar::CVar( const char* _name, const char* _type )
{
	struct typePair_t
	{
		std::string	typeName;
		Type		type;
	};

	typePair_t types[ 3 ] =
	{
		{ "bool",	Type::BOOL },
		{ "int",	Type::INT },
		{ "char*",	Type::STRING },
	};

	for ( size_t i = 0; i < 3; ++i )
	{
		if ( Equals( std::string( _type ), types[ i ].typeName ) )
		{
			type = types[ i ].type;
			break;
		}
	}
	memset( (void*)&value, 0, sizeof( value ) );

	valid = false;
	name = _name;
	cvarMap.push_back( this );
}


CVar* CVar::Search( const std::string& searchStr )
{
	for ( size_t i = 0; i < cvarMap.size(); ++i ) {
		if ( Equals( searchStr, cvarMap[ i ]->GetName() ) ) {
			return cvarMap[ i ];
		}
	}
	return nullptr;
}