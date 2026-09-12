#pragma once

#include <SysCore/log.h>
#include <cstdint>
#include <string>


enum class logSeverity_t : uint8_t
{
	Verbose,
	Info,
	Warning,
	Error,
};

struct logRecord_t
{
	const char*		system;
	logSeverity_t	severity;
	std::string		message;
};

extern SysCore::Logger<logRecord_t> g_log;

void LogMsg( const char* system, logSeverity_t severity, const char* fmt, ... );
