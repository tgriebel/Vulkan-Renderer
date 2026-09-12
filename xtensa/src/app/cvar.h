#pragma once

#include <vector>
#include <assert.h>
#include <syscore/systemUtils.h>

class CVar
{
public:
	enum class Type : uint8_t
	{
		BOOL,
		INT,
		FLOAT,
		STRING,
	};

	static constexpr uint32_t MaxStringLength = 64;

private:

	Type type;
	std::string name;
	bool valid;

	struct Value
	{
		union {
			bool		asBool;
			int32_t		asInt;
			float		asFloat;
			char		asStr[ MaxStringLength ];
		};
	} value;

	void Init( const char* _name, CVar::Type _type );

public:
	CVar( const char* _name, CVar::Type _type, bool value );
	CVar( const char* _name, CVar::Type _type, int32_t value );
	CVar( const char* _name, CVar::Type _type, float value );
	CVar( const char* _name, CVar::Type _type, const char* value );

	inline bool IsBool() const
	{
		return type == CVar::Type::BOOL;
	}

	inline bool IsInt() const
	{
		return type == CVar::Type::INT;
	}

	inline bool IsFloat() const
	{
		return type == CVar::Type::FLOAT;
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

	float GetFloat() const
	{
		assert( type == Type::FLOAT );
		return value.asFloat;
	}

	const char* GetString() const
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

	void Set( const float number )
	{
		assert( type == Type::FLOAT );
		if ( type == Type::FLOAT )
		{
			valid = true;
			value.asFloat = number;
		}
	}

	void Set( const char* string )
	{
		assert( type == Type::STRING );
		assert( strlen( string ) < MaxStringLength );
		if ( type == Type::STRING )
		{
			valid = true;
			strcpy_s( value.asStr, string );
		}
	}

	static bool ParseCommand( const std::string& command );
	static CVar* Search( const std::string& searchStr );
};
#define MakeCVar(TypeName, Name, Value) CVar Name = CVar( #Name, CVar::Type::##TypeName, Value );