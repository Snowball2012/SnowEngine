#include "CVars.h"

#include "CoreMisc.h"

#include <cctype>

ConsoleCommand* g_console_commands_head = nullptr;

CONSOLE_COMMAND_DEFINE( echo, "Echo arguments to the console and log" )
{
    output.clear();
    for ( const std::string& arg : args )
    {
        if ( !output.empty() )
            output += ' ';
        output += arg;
    }
    return true;
}

void Console::AddCVars( ConsoleVariableBase* cvar_head )
{
    for ( ConsoleVariableBase* i = cvar_head; i != nullptr; i = i->GetNext() )
    {
        std::string key = GetKey( i->GetName() );

        m_cvars[key] = i;

        auto deferred_value_it = m_deferred_cvars.find( key );
        if ( deferred_value_it != m_deferred_cvars.end() )
        {
            if ( !i->SetValue( deferred_value_it->second.c_str() ) )
            {
                std::string preserved_value;
                i->GetValue( preserved_value );
                SE_LOG_ERROR( Core,
                    "Can't override initial value for cvar %s: \"%s\", preserving original value \"%s\"",
                    i->GetName(),
                    deferred_value_it->second.c_str(),
                    preserved_value.c_str() );
            }

            m_deferred_cvars.erase( deferred_value_it );
        }
    }
}

void Console::AddCVarInitialValueOverride( const char* name, const char* value )
{
    std::string key = GetKey( name );

    // Lookup existing cvars, if not found, put into deferred init map
    auto existing_cvar_it = m_cvars.find( key );
    if ( existing_cvar_it != m_cvars.end() )
    {
        if ( !existing_cvar_it->second->SetValue( value ) )
        {
            std::string preserved_value;
            existing_cvar_it->second->GetValue( preserved_value );
            SE_LOG_ERROR( Core,
                "Can't override initial value for cvar %s: \"%s\", preserving original value \"%s\"",
                existing_cvar_it->second->GetName(),
                value,
                preserved_value.c_str() );
        }
        return;
    }

    m_deferred_cvars[key] = value;
}

void Console::AddCommands( ConsoleCommand* command_head )
{
    for ( ConsoleCommand* command = command_head; command != nullptr; command = command->GetNext() )
    {
        m_commands[GetKey( command->GetName() )] = command;
    }
}

std::string Console::GetKey( const char* cvar_name )
{
    std::string name_str = cvar_name;
    for ( char& c : name_str )
    {
        c = std::tolower( c );
    }
    return name_str;
}

bool Console::ProcessCommand( const char* cmd )
{
    std::vector<std::string> tokens;
    std::string output;
    if ( !Tokenize( cmd, tokens, output ) )
    {
        AddOutput( "Error: " + output );
        SE_LOG_ERROR( Core, "Console parse error: %s", output.c_str() );
        return false;
    }

    if ( tokens.empty() )
        return true;

    AddOutput( "> " + std::string( cmd ) );

    const std::string key = GetKey( tokens[0].c_str() );
    if ( key == "listcvars" )
    {
        ListAllCVars();
        AddOutput( "CVar list written to the log" );
        return true;
    }
    if ( key == "listcommands" )
    {
        ListAllCommands();
        AddOutput( "Command list written to the log" );
        return true;
    }

    const auto command_it = m_commands.find( key );
    if ( command_it != m_commands.end() )
    {
        std::vector<std::string> args( std::next( tokens.begin() ), tokens.end() );
        const bool success = command_it->second->Execute( args, output );
        if ( output.empty() )
            output = success ? "Command completed" : "Command failed";
        AddOutput( output );
        if ( success )
            SE_LOG_INFO( Core, "Console command %s completed: %s", command_it->second->GetName(), output.c_str() );
        else
            SE_LOG_ERROR( Core, "Console command %s failed: %s", command_it->second->GetName(), output.c_str() );
        return success;
    }

    const auto cvar_it = m_cvars.find( key );
    if ( cvar_it != m_cvars.end() )
    {
        if ( tokens.size() == 1 )
        {
            cvar_it->second->GetValue( output );
            output = std::string( cvar_it->second->GetName() ) + " = \"" + output + "\"";
            AddOutput( output );
            SE_LOG_INFO( Core, "%s", output.c_str() );
            return true;
        }

        return SetCVarValue( tokens[0].c_str(), tokens[1].c_str(), output );
    }

    output = "Unknown command or CVar: " + tokens[0];
    AddOutput( "Error: " + output );
    SE_LOG_ERROR( Core, "%s", output.c_str() );
    return false;
}

bool Console::HasCVar( const char* name ) const
{
    return name && m_cvars.contains( GetKey( name ) );
}

bool Console::HasCommand( const char* name ) const
{
    if ( !name )
        return false;
    const std::string key = GetKey( name );
    return key == "listcvars" || key == "listcommands" || m_commands.contains( key );
}

bool Console::GetCVarValue( const char* name, std::string& value ) const
{
    if ( !name )
        return false;
    const auto it = m_cvars.find( GetKey( name ) );
    if ( it == m_cvars.end() )
        return false;
    it->second->GetValue( value );
    return true;
}

bool Console::SetCVarValue( const char* name, const char* value, std::string& output )
{
    if ( !name || !value )
    {
        output = "CVar name and value are required";
        AddOutput( "Error: " + output );
        return false;
    }

    const auto it = m_cvars.find( GetKey( name ) );
    if ( it == m_cvars.end() )
    {
        output = "Unknown CVar: " + std::string( name );
        AddOutput( "Error: " + output );
        return false;
    }

    if ( !it->second->SetValue( value ) )
    {
        output = "Invalid value for CVar " + std::string( it->second->GetName() ) + ": " + value;
        AddOutput( "Error: " + output );
        SE_LOG_ERROR( Core, "%s", output.c_str() );
        return false;
    }

    std::string current_value;
    it->second->GetValue( current_value );
    output = std::string( it->second->GetName() ) + " = \"" + current_value + "\"";
    AddOutput( output );
    SE_LOG_INFO( Core, "%s", output.c_str() );
    return true;
}

void Console::AddOutput( std::string_view line )
{
    std::scoped_lock lock( m_output_mutex );
    m_output.emplace_back( line );
    constexpr size_t max_output_lines = 4096;
    if ( m_output.size() > max_output_lines )
        m_output.erase( m_output.begin(), m_output.begin() + ( m_output.size() - max_output_lines ) );
}

std::vector<std::string> Console::GetOutputSnapshot() const
{
    std::scoped_lock lock( m_output_mutex );
    return m_output;
}

void Console::ClearOutput()
{
    std::scoped_lock lock( m_output_mutex );
    m_output.clear();
}

void Console::LogCallback( const char* line, void* user_data )
{
    if ( line && user_data )
        static_cast<Console*>( user_data )->AddOutput( line );
}

bool Console::Tokenize( const char* command_line, std::vector<std::string>& tokens, std::string& error )
{
    if ( !command_line )
    {
        error = "Command line is null";
        return false;
    }

    std::string token;
    bool in_quotes = false;
    bool escaping = false;
    for ( const char* cursor = command_line; *cursor; ++cursor )
    {
        const char c = *cursor;
        if ( escaping )
        {
            token.push_back( c );
            escaping = false;
        }
        else if ( c == '\\' && in_quotes )
        {
            escaping = true;
        }
        else if ( c == '"' )
        {
            in_quotes = !in_quotes;
        }
        else if ( std::isspace( static_cast<unsigned char>( c ) ) && !in_quotes )
        {
            if ( !token.empty() )
            {
                tokens.emplace_back( std::move( token ) );
                token.clear();
            }
        }
        else
        {
            token.push_back( c );
        }
    }

    if ( escaping || in_quotes )
    {
        error = "Unterminated quoted argument";
        return false;
    }
    if ( !token.empty() )
        tokens.emplace_back( std::move( token ) );
    return true;
}

void Console::ListAllCVars() const
{
    SE_LOG_INFO( Core, "Listing all cvars with their values" );
    int i = 0;
    for ( const auto& [cvar_key, cvar] : m_cvars )
    {
        std::string value;
        cvar->GetValue( value );
        SE_LOG_INFO( Core, "\t%d: %s = \"%s\" Description: %s", i, cvar->GetName(), value.c_str(), cvar->GetDecription() );
        i++;
    }

    SE_LOG_INFO( Core, "Total: %d cvars", i );
}

void Console::ListAllCommands() const
{
    SE_LOG_INFO( Core, "Listing all console commands" );
    int i = 0;
    for ( const auto& [command_key, command] : m_commands )
    {
        SE_LOG_INFO( Core, "\t%d: %s - %s", i, command->GetName(), command->GetDescription() );
        ++i;
    }
    SE_LOG_INFO( Core, "\t%d: listcvars - List all registered CVars", i++ );
    SE_LOG_INFO( Core, "\t%d: listcommands - List all registered commands", i++ );
    SE_LOG_INFO( Core, "Total: %d commands", i );
}
