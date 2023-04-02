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
