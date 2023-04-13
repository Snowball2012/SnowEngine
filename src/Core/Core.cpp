#include "Core.h"

std::string ToOSPath( const char* internal_path )
{
	// todo: move to core
	std::string result;

	if ( !internal_path )
		return result;

	static constexpr const char* engine_prefix = "#engine/";
	constexpr size_t engine_prefix_len = std::string_view( engine_prefix ).size();

	if ( !_strnicmp( internal_path, engine_prefix, engine_prefix_len ) )
	{
		result = g_core_paths.engine_content + ( internal_path + engine_prefix_len );
	}
	else
	{
		result = internal_path;
	}

	return result;
}

Logger::Logger( const Logger::CreateInfo& create_info )
	: m_write_to_std( create_info.mirror_to_stdout )
{
	if ( create_info.path != nullptr && create_info.path[0] != '\0' )
	{
		m_file.emplace( create_info.path );
		if ( !m_file->good() )
		{
			m_file = std::nullopt;
			std::cerr << "[Error][Core]: couldn't create or open a log file at path: " << create_info.path << std::endl;
		}
		else
		{
			std::cout << "[Core]: using log file path: " << create_info.path << std::endl;
		}
	}
	else
	{
		std::cout << "[Core]: no log path specified" << std::endl;
	}
}

Logger::~Logger()
{
	Flush();
}

void Logger::Flush()
{
	if ( m_write_to_std )
	{
		std::flush( std::cout );
		std::flush( std::cerr );
	}
	if ( m_file )
	{
		std::flush( *m_file );
	}
}

void Logger::Log( const LogCategory& category, LogMessageType msg_type, const char* fmt, ... )
{
	if ( !SE_ENSURE( fmt ) )
		return;

	std::stringstream message;
	if ( msg_type != LogMessageType::Info )
	{
		message << "[" << TypeToString( msg_type ) << "]";
	}
	message << "[" << category.name << "]: ";

	constexpr size_t buffer_size = 16 * SizeKB;
	char buf[buffer_size];

	va_list args;
	va_start( args, fmt );
	if ( !SE_ENSURE( vsprintf_s( buf, fmt, args ) != -1 ) )
	{
		std::cerr << "[Error][Core]: failed to log a message" << std::endl;
	}
	va_end( args );

	message << buf << "\n";

	if ( m_file && m_file->good() )
	{
		( *m_file ) << message.str();
	}
	if ( m_write_to_std )
	{
		if ( msg_type == LogMessageType::Error )
		{
			std::cerr << message.str();
		}
		else
		{
			std::cout << message.str();
		}
	}
}

void Logger::Newline()
{
	if ( m_file && m_file->good() )
	{
		( *m_file ) << '\n';
	}
	if ( m_write_to_std )
	{
		std::cout << '\n';
	}
}

const char* Logger::TypeToString( LogMessageType type )
{
	switch ( type )
	{
		case LogMessageType::Info: return "Info";
		case LogMessageType::Warning: return "Warning";
		case LogMessageType::Error: return "Error";
	}

	NOTIMPL;
}
