#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <iostream>

SysCore::Logger<logRecord_t> g_log;

static const char* SeverityName( logSeverity_t severity )
{
	switch ( severity )
	{
		case logSeverity_t::Verbose:	return "Verbose";
		case logSeverity_t::Info:		return "Info";
		case logSeverity_t::Warning:	return "Warning";
		case logSeverity_t::Error:		return "Error";
		default:						return "Unknown";
	}
}


void LogMsg( const char* system, logSeverity_t severity, const char* fmt, ... )
{
	char buf[ 2048 ];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );

	std::cerr << "[" << system << "] [" << SeverityName( severity ) << "] " << buf << "\n";

	logRecord_t& rec = g_log.NewEntry();
	rec.system   = system;
	rec.severity = severity;
	rec.message  = buf;
}
