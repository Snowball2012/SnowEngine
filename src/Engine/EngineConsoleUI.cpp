#include "StdAfx.h"

#include "EngineConsoleUI.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

EngineConsoleUI::EngineConsoleUI( Console& console )
    : m_console( console )
{
}

void EngineConsoleUI::Toggle()
{
    m_open = !m_open;
}

void EngineConsoleUI::Draw()
{
    if ( !m_open )
        return;

    ImGui::SetNextWindowSize( ImVec2( 720.0f, 420.0f ), ImGuiCond_FirstUseEver );
    if ( !ImGui::Begin( "SnowEngine Console", &m_open ) )
    {
        ImGui::End();
        return;
    }

    if ( ImGui::Button( "Clear" ) )
        m_console.ClearOutput();
    ImGui::SameLine();
    ImGui::TextUnformatted( "Enter a command, or a CVar name and optional value." );

    const float footer_height = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild( "ConsoleOutput", ImVec2( 0, -footer_height ), true, ImGuiWindowFlags_HorizontalScrollbar );
    m_console.ReadOutput( []( std::span<const std::string> output )
    {
        for ( const std::string& line : output )
            ImGui::TextUnformatted( line.c_str() );
    } );
    if ( m_scroll_to_bottom )
    {
        ImGui::SetScrollHereY( 1.0f );
        m_scroll_to_bottom = false;
    }
    ImGui::EndChild();

    const ImGuiID console_input_id = ImGui::GetID( "##ConsoleInput" );
    if ( !m_autocomplete_options.empty()
        && ImGui::GetActiveID() == console_input_id
        && ImGui::IsKeyPressed( ImGuiKey_Escape, false ) )
    {
        DisableAutocomplete();
        const ImGuiID escape_owner_id = ImGui::GetID( "##ConsoleAutocompleteEscape" );
        ImGui::SetKeyOwner( ImGuiKey_Escape, escape_owner_id, ImGuiInputFlags_LockThisFrame );
    }

    ImGui::SetNextItemWidth( -1.0f );
    constexpr ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_CallbackCharFilter
        | ImGuiInputTextFlags_CallbackCompletion
        | ImGuiInputTextFlags_CallbackHistory
        | ImGuiInputTextFlags_CallbackEdit;
    const bool submitted = ImGui::InputText(
        "##ConsoleInput",
        &m_input,
        input_flags,
        &EngineConsoleUI::InputTextCallback,
        this );
    const ImVec2 input_min = ImGui::GetItemRectMin();
    const ImVec2 input_size = ImGui::GetItemRectSize();
    const bool input_active = ImGui::IsItemActive();
    if ( ImGui::IsWindowAppearing() )
        ImGui::SetKeyboardFocusHere( -1 );
    if ( submitted )
    {
        if ( !m_input.empty() )
        {
            m_console.QueueCommand( m_input );
            AddToHistory( m_input );
        }
        m_input.clear();
        m_autocomplete_options.clear();
        m_autocomplete_selection = 0;
        m_autocomplete_suppressed = false;
        ResetHistoryNavigation();
        m_scroll_to_bottom = true;
        ImGui::SetKeyboardFocusHere( -1 );
    }

    if ( input_active && !m_autocomplete_options.empty() )
    {
        constexpr float autocomplete_alpha = 0.86f;
        const float autocomplete_max_height = ImGui::GetTextLineHeightWithSpacing() * 10.0f
            + ImGui::GetStyle().WindowPadding.y * 2.0f;
        ImGui::SetNextWindowPos( input_min, ImGuiCond_Always, ImVec2( 0.0f, 1.0f ) );
        ImGui::SetNextWindowBgAlpha( autocomplete_alpha );
        ImGui::SetNextWindowSizeConstraints(
            ImVec2( input_size.x, 0.0f ),
            ImVec2( input_size.x, autocomplete_max_height ) );
        constexpr ImGuiWindowFlags autocomplete_flags = ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_Tooltip;

        if ( ImGui::Begin( "##ConsoleAutocomplete", nullptr, autocomplete_flags ) )
        {
            for ( size_t i = 0; i < m_autocomplete_options.size(); ++i )
            {
                const bool selected = i == m_autocomplete_selection;
                ImGui::Selectable( m_autocomplete_options[i].c_str(), selected );
                if ( selected && m_autocomplete_scroll_to_selection )
                    ImGui::SetScrollHereY( 0.5f );
            }
        }
        ImGui::End();
        m_autocomplete_scroll_to_selection = false;
    }

    ImGui::End();
}

void EngineConsoleUI::RefreshAutocomplete( std::string_view input )
{
    const std::string selected_option = m_autocomplete_options.empty()
        ? std::string()
        : m_autocomplete_options[m_autocomplete_selection];

    if ( m_autocomplete_suppressed || input.find_first_of( " \t\r\n" ) != std::string_view::npos )
    {
        m_autocomplete_options.clear();
        m_autocomplete_selection = 0;
        return;
    }

    m_console.GetAutocompleteOptions( input, m_autocomplete_options );
    m_autocomplete_selection = 0;
    if ( !selected_option.empty() )
    {
        const auto selected_it = std::find(
            m_autocomplete_options.begin(),
            m_autocomplete_options.end(),
            selected_option );
        if ( selected_it != m_autocomplete_options.end() )
            m_autocomplete_selection = static_cast<size_t>( selected_it - m_autocomplete_options.begin() );
    }
}

void EngineConsoleUI::DisableAutocomplete()
{
    m_autocomplete_suppressed = true;
    m_autocomplete_options.clear();
    m_autocomplete_selection = 0;
    m_autocomplete_scroll_to_selection = false;
}

void EngineConsoleUI::AddToHistory( std::string_view command )
{
    if ( m_history.size() == max_history_entries )
        m_history.erase( m_history.begin() );
    m_history.emplace_back( command );
}

void EngineConsoleUI::NavigateHistory( ImGuiInputTextCallbackData* data, bool older )
{
    if ( m_history.empty() )
        return;

    if ( older )
    {
        if ( m_history_selection == no_history_selection )
        {
            m_history_draft.assign( data->Buf, data->BufTextLen );
            m_history_selection = m_history.size() - 1;
        }
        else if ( m_history_selection > 0 )
        {
            --m_history_selection;
        }
    }
    else
    {
        if ( m_history_selection == no_history_selection )
            return;
        if ( m_history_selection + 1 < m_history.size() )
            ++m_history_selection;
        else
            m_history_selection = no_history_selection;
    }

    const std::string& history_input = m_history_selection == no_history_selection
        ? m_history_draft
        : m_history[m_history_selection];
    data->DeleteChars( 0, data->BufTextLen );
    data->InsertChars( 0, history_input.c_str() );
    DisableAutocomplete();
}

void EngineConsoleUI::ResetHistoryNavigation()
{
    m_history_selection = no_history_selection;
    m_history_draft.clear();
}

int EngineConsoleUI::HandleInput( ImGuiInputTextCallbackData* data )
{
    if ( data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter )
    {
        m_autocomplete_suppressed = false;
        ResetHistoryNavigation();
    }
    else if ( data->EventFlag == ImGuiInputTextFlags_CallbackEdit )
    {
        RefreshAutocomplete( std::string_view( data->Buf, data->BufTextLen ) );
    }
    else if ( data->EventFlag == ImGuiInputTextFlags_CallbackHistory )
    {
        if ( !m_autocomplete_options.empty() )
        {
            if ( data->EventKey == ImGuiKey_UpArrow )
            {
                if ( m_autocomplete_selection == 0 )
                    m_autocomplete_selection = m_autocomplete_options.size() - 1;
                else
                    --m_autocomplete_selection;
            }
            else if ( data->EventKey == ImGuiKey_DownArrow )
            {
                m_autocomplete_selection = ( m_autocomplete_selection + 1 )
                    % m_autocomplete_options.size();
            }
            m_autocomplete_scroll_to_selection = true;
        }
        else if ( data->EventKey == ImGuiKey_UpArrow || data->EventKey == ImGuiKey_DownArrow )
        {
            NavigateHistory( data, data->EventKey == ImGuiKey_UpArrow );
        }
    }
    else if ( data->EventFlag == ImGuiInputTextFlags_CallbackCompletion )
    {
        if ( m_autocomplete_suppressed )
        {
            m_autocomplete_suppressed = false;
            ResetHistoryNavigation();
            RefreshAutocomplete( std::string_view( data->Buf, data->BufTextLen ) );
            return 0;
        }

        RefreshAutocomplete( std::string_view( data->Buf, data->BufTextLen ) );
        if ( !m_autocomplete_options.empty() )
        {
            const std::string completion = m_autocomplete_options[m_autocomplete_selection];
            data->DeleteChars( 0, data->BufTextLen );
            data->InsertChars( 0, completion.c_str() );
            data->InsertChars( data->CursorPos, " " );
            m_autocomplete_options.clear();
            m_autocomplete_selection = 0;
            ResetHistoryNavigation();
        }
    }
    return 0;
}

int EngineConsoleUI::InputTextCallback( ImGuiInputTextCallbackData* data )
{
    return static_cast<EngineConsoleUI*>( data->UserData )->HandleInput( data );
}
