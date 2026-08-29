#pragma once

#include "CoreMisc.h"

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// All cvars are supposed to be statically initialized
class ConsoleVariableBase;

extern ConsoleVariableBase* g_cvars_head;

class ConsoleVariableBase
{
private:
    const char* m_name = nullptr;
    const char* m_description = nullptr;

    ConsoleVariableBase* m_next = nullptr;

public:
    ConsoleVariableBase( const char* name, const char* description )
    { 
        m_name = name;
        m_next = g_cvars_head;
        m_description = description;
        g_cvars_head = this;
    }
    virtual ~ConsoleVariableBase() {}

    const char* GetName() const { return m_name; }
    const char* GetDecription() const { return m_description; }

    virtual void GetValue( std::string& value ) const = 0;
    virtual bool SetValue( const char* value ) = 0;

    ConsoleVariableBase* GetNext() const { return m_next; }
};

template<typename T>
class ConsoleVariable : public ConsoleVariableBase
{
private:
    T m_value;
public:
    ConsoleVariable( const char* name, const char* description, const T& initial_value )
        : ConsoleVariableBase( name, description )
        , m_value( initial_value )
    {
    }

    const T& GetValue() const { return m_value; }
    void SetValue( T&& value ) { m_value = std::forward<T>( value ); }

    virtual void GetValue( std::string& str_value ) const override { str_value = std::to_string( m_value ); }
    virtual bool SetValue( const char* str_value ) override { return SetFromString( str_value, m_value ); }
};

#define CVAR_DEFINE( name, type, default_value, description ) ConsoleVariable<type> name = ConsoleVariable<type>( S_(name), description, default_value )

#define CVAR_EXTERN( name, type ) extern ConsoleVariable<type> name

class ConsoleCommand;
extern ConsoleCommand* g_console_commands_head;

using ConsoleCommandCallback = bool (*)( const std::vector<std::string>& args, std::string& output );

class ConsoleCommand
{
private:
    const char* m_name = nullptr;
    const char* m_description = nullptr;
    ConsoleCommandCallback m_callback = nullptr;
    ConsoleCommand* m_next = nullptr;

public:
    ConsoleCommand( const char* name, const char* description, ConsoleCommandCallback callback )
        : m_name( name )
        , m_description( description )
        , m_callback( callback )
        , m_next( g_console_commands_head )
    {
        g_console_commands_head = this;
    }

    const char* GetName() const { return m_name; }
    const char* GetDescription() const { return m_description; }
    ConsoleCommand* GetNext() const { return m_next; }

    bool Execute( const std::vector<std::string>& args, std::string& output ) const
    {
        return m_callback && m_callback( args, output );
    }
};

#define CONSOLE_COMMAND_DEFINE( name, description ) \
    bool name ## _ConsoleCommandImpl( const std::vector<std::string>& args, std::string& output ); \
    ConsoleCommand name( S_( name ), description, &name ## _ConsoleCommandImpl ); \
    bool name ## _ConsoleCommandImpl( const std::vector<std::string>& args, std::string& output )

class Console
{
private:
    // @todo - trie
    std::unordered_map<std::string, ConsoleVariableBase*> m_cvars;

    std::unordered_map<std::string, std::string> m_deferred_cvars;

    std::unordered_map<std::string, ConsoleCommand*> m_commands;

    mutable std::mutex m_output_mutex;
    std::vector<std::string> m_output;

public:
    // This can be used to allow override cvar values with command line arguments
    void AddCVarInitialValueOverride( const char* name, const char* value );

    void AddCVars( ConsoleVariableBase* cvar_head );
    void AddCommands( ConsoleCommand* command_head );
    bool ProcessCommand( const char* cmd );

    bool HasCVar( const char* name ) const;
    bool HasCommand( const char* name ) const;
    bool GetCVarValue( const char* name, std::string& value ) const;
    bool SetCVarValue( const char* name, const char* value, std::string& output );

    void AddOutput( std::string_view line );
    std::vector<std::string> GetOutputSnapshot() const;
    void ClearOutput();

    static void LogCallback( const char* line, void* user_data );

    void ListAllCVars() const;
    void ListAllCommands() const;

private:
    static std::string GetKey( const char* cvar_name );
    static bool Tokenize( const char* command_line, std::vector<std::string>& tokens, std::string& error );
};
