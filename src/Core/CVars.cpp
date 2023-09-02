#include "CVars.h"

#include "CoreMisc.h"

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
    NOTIMPL;
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