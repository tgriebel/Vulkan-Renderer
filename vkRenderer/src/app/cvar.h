#pragma once

#include <vector>
#include <assert.h>
#include <syscore/systemUtils.h>

class CVar
{
private:

	enum class Type : uint8_t
	{
		BOOL,
		INT,
		STRING,
	} type;

	std::string name;
	bool valid;

	struct Value
	{
		union {
			bool		asBool;
			int			asInt;
			char		asStr[ 32 ];
		};
	} value;

public:
	CVar( const char* _name, const char* _type );

	inline bool IsBool() const
	{
		return type == CVar::Type::BOOL;
	}

	inline bool IsInt() const
	{
		return type == CVar::Type::INT;
	}

	inline bool IsString() const
	{
		return type == CVar::Type::STRING;
	}

	inline bool IsValid() const
	{
		return valid;
	}

	bool GetBool() const
	{
		assert( type == Type::BOOL );
		return value.asBool;
	}

	int GetInt() const
	{
		assert( type == Type::INT );
		return value.asInt;
	}

	std::string GetString() const
	{
		assert( type == Type::STRING );
		return value.asStr;
	}

	const std::string& GetName() const
	{
		return name;
	}

	void Set( const bool flag )
	{
		assert( type == Type::BOOL );
		if ( type == Type::BOOL )
		{
			valid = true;
			value.asBool = flag;
		}
	}

	void Set( const int number )
	{
		assert( type == Type::INT );
		if ( type == Type::INT )
		{
			valid = true;
			value.asInt = number;
		}
	}

	void Set( const char* string )
	{
		assert( type == Type::STRING );
		assert( strlen( string ) < 32 );
		if ( type == Type::STRING )
		{
			valid = true;
			strcpy_s( value.asStr, string );
		}
	}

	static CVar* Search( const std::string& searchStr );
};
#define MakeCVar(Type, Name) CVar Name = CVar( #Name, #Type );