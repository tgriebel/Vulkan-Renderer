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
	name = ToLower( _name );
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


bool CVar::ParseCommand( const std::string& command )
{
	using namespace std::string_literals;

	std::string cmdStr = command;
	Trim( cmdStr );
	ToLower( cmdStr );

	// Flags
	if ( HasPrefix( cmdStr, "-"s ) )
	{
		CVar* v = CVar::Search( cmdStr.substr( 1 ) );
		if ( v == nullptr || ( v->IsBool() == false ) ) {
			return false;
		}
		v->Set( true );
	}
	// Everything else
	else
	{
		const size_t offset = cmdStr.find( "=" );
		if ( offset == 0 ) {
			return false;
		}

		CVar* v = CVar::Search( cmdStr.substr( 0, offset ) );
		if ( v == nullptr ) {
			return false;
		}
		if ( v->IsBool() )
		{
			std::string arg = cmdStr.substr( offset + 1 );
			if ( arg == "1" || arg == "true" ) {
				v->Set( true );
			}
			else {
				v->Set( false );
			}
		}
		else if ( v->IsInt() ) {
			v->Set( std::stoi( cmdStr.substr( offset + 1 ) ) );
		}
		else if ( v->IsString() ) {
			v->Set( cmdStr.substr( offset + 1 ).c_str() );
		}
		else {
			return false;
		}
	}
	return true;
}