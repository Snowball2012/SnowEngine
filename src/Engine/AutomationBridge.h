#pragma once

#include <filesystem>

class Console;

// A local file-based bridge used by the SnowEngine MCP server. Requests are
// consumed from the engine main thread so console commands can safely interact
// with renderer and world state.
class AutomationBridge
{
private:
    Console& m_console;
    std::filesystem::path m_requests_path;
    std::filesystem::path m_responses_path;
    bool m_ready = false;

public:
    AutomationBridge( Console& console, const std::filesystem::path& engine_root );

    bool IsReady() const { return m_ready; }
    void Tick();

private:
    void ProcessRequest( const std::filesystem::path& request_path );
};
