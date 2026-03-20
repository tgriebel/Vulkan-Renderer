#include "cvar.h"

static std::vector<CVar*> cvarMap;

void CVar::Init( const char* _name, CVar::Type _type )
{
	struct typePair_t
	{
		std::string	typeName;
		Type		type;
	};

	type = _type;

	memset( (void*)&value, 0, sizeof( value ) );

	valid = false;
	name = ToLower( _name );
	cvarMap.push_back( this );
}


CVar::CVar( const char* _name, CVar::Type _type, bool _value )
{
	Init( _name, _type );

	Set( _value );
}


CVar::CVar( const char* _name, CVar::Type _type, int32_t _value )
{
	Init( _name, _type );

	Set( _value );
}


CVar::CVar( const char* _name, CVar::Type _type, const float _value )
{
	Init( _name, _type );

	Set( _value );
}


CVar::CVar( const char* _name, CVar::Type _type, const char* _value )
{
	Init( _name, _type );

	Set( _value );
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

	// Flags
	if ( HasPrefix( cmdStr, "-"s ) )
	{
		CVar* v = CVar::Search( cmdStr.substr( 1 ) );
		if ( v == nullptr || ( v->IsBool() == false ) ) {
			return false;
		}
		v->Set( true );

		return true;
	}

	// Everything else
	const size_t offset = cmdStr.find( "=" );
	if ( offset == std::string::npos || offset == 0 ) {
		return false;
	}

	std::string varName = cmdStr.substr( 0, offset );
	ToLower( cmdStr );

	CVar* v = CVar::Search( varName );
	if ( v == nullptr ) {
		return false;
	}

	// Value is case-sensitive
	std::string arg = cmdStr.substr( offset + 1 );
	Trim( arg );

	if ( arg.empty() ) {
		return false;
	}

	if ( v->IsBool() )
	{
		v->Set( arg == "1" || arg == "true" );
	}
	else if ( v->IsInt() )
	{
		v->Set( std::stoi( arg ) );
	}
	else if ( v->IsFloat() )
	{
		v->Set( std::stof( arg ) );
	}
	else if ( v->IsString() )
	{
		if ( arg.size() >= CVar::MaxStringLength ) {
			return false;
		}
		v->Set( arg.c_str() );
	}
	else
	{
		return false;
	}
	return true;
}