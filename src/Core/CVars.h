#pragma once

#include "CoreMisc.h"

#include <string>

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

class Console
{
private:
    // @todo - trie
    std::unordered_map<std::string, ConsoleVariableBase*> m_cvars;

    std::unordered_map<std::string, std::string> m_deferred_cvars;

public:
    // This can be used to allow override cvar values with command line arguments
    void AddCVarInitialValueOverride( const char* name, const char* value );

    void AddCVars( ConsoleVariableBase* cvar_head );
    bool ProcessCommand( const char* cmd );

    void ListAllCVars() const;

private:
    static std::string GetKey( const char* cvar_name );
};