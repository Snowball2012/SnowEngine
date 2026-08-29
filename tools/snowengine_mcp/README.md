# SnowEngine MCP server

This local MCP server lets an automation client execute SnowEngine console
commands, read and change CVars, and retrieve engine logs. The engine processes
requests from its main thread using atomic files under
`<engine-root>/.snowengine_mcp`; no network port or external Python package is
required.

## Run

Start Sandbox normally:

```powershell
D:/snow_engine/bin/Debug/Sandbox.exe -LogPath Engine.log -EnginePath D:/snow_engine -StartLevel '#engine/Levels/Default.sel'
```

Configure an MCP client to launch:

```powershell
python D:/snow_engine/tools/snowengine_mcp/server.py --engine-path D:/snow_engine
```

The server exposes:

- `ping`
- `execute_console_command`
- `get_cvar`
- `set_cvar`
- `get_logs`

Requests time out after 10 seconds by default. Override this with
`--timeout <seconds>`. The bridge directory is runtime state and should not be
committed.
