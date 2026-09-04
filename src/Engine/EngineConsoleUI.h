#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class Console;
struct ImGuiInputTextCallbackData;

class EngineConsoleUI
{
private:
    static constexpr size_t no_history_selection = static_cast<size_t>( -1 );
    static constexpr size_t max_history_entries = 256;

    Console& m_console;
    bool m_open = false;
    bool m_scroll_to_bottom = false;
    std::string m_input;

    std::vector<std::string> m_autocomplete_options;
    size_t m_autocomplete_selection = 0;
    bool m_autocomplete_scroll_to_selection = false;
    bool m_autocomplete_suppressed = false;

    std::vector<std::string> m_history;
    size_t m_history_selection = no_history_selection;
    std::string m_history_draft;

public:
    explicit EngineConsoleUI( Console& console );

    void Toggle();
    void Draw();

private:
    void RefreshAutocomplete( std::string_view input );
    void DisableAutocomplete();
    void AddToHistory( std::string_view command );
    void NavigateHistory( ImGuiInputTextCallbackData* data, bool older );
    void ResetHistoryNavigation();
    int HandleInput( ImGuiInputTextCallbackData* data );
    static int InputTextCallback( ImGuiInputTextCallbackData* data );
};
