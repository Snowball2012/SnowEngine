# SnowEngine Development Guide

## Scope and architecture

SnowEngine is a small real-time path tracer built as a Microsoft Visual Studio 2022 solution. Keep changes incremental and compatible with the existing engine architecture. Changes to core engine architecture must be discussed with the user before implementation.

The executable engine/renderer project is `Sandbox.vcxproj`.

## Agent workspace rules

- The primary agent may only read or write inside `D:/snow_engine`.
- Every subagent must use a separate Git worktree under `I:/snow_engine_agent_worktrees/<agent-name>` and may only touch its own worktree. Creating and working inside that assigned worktree is the sole exception to the `D:/snow_engine` boundary.
- Before editing, confirm that the current branch is not `master`. Commits are allowed only on non-`master` branches.
- Never push changes to a remote repository.
- Preserve existing user changes. Do not reset, discard, overwrite, or include unrelated changes in commits.
- Prefer simpler models and lower reasoning effort for subagents; keep high-effort reasoning in the orchestrating agent.
- Downloads are allowed up to 64 GB total for the task. Ask the user before exceeding that limit.
- Do not make significant core-architecture changes without discussing them with the user first.

## Build

Use a Visual Studio 2022 Developer PowerShell or Developer Command Prompt so MSBuild and the MSVC toolchain are available.

Build the executable and all dependencies through the solution:

```powershell
msbuild snow_engine.sln /m /t:Sandbox /p:Configuration=Debug /p:Platform=x64
```

Building `msvs/Sandbox/Sandbox.vcxproj` directly does not define the repository
`SolutionDir` correctly for its pre-build DLL copies. If a direct project
build is needed, provide it explicitly:

```powershell
msbuild msvs/Sandbox/Sandbox.vcxproj /m /p:SolutionDir=D:/snow_engine/ /p:Configuration=Debug /p:Platform=x64
```

Use `Release` in place of `Debug` when validating optimized rendering or
performance. Sandbox is written to `bin/<Configuration>/Sandbox.exe`.

## Launch

SnowEngine requires an engine content root and a start level. The standard arguments are:

```text
-LogPath Engine.log -EnginePath D:/snow_engine -StartLevel #engine/Levels/Default.sel
```

Launch the built Sandbox executable from a working directory compatible with its runtime DLL and shader dependencies:

```powershell
<path-to-Sandbox.exe> -LogPath Engine.log -EnginePath D:/snow_engine -StartLevel '#engine/Levels/Default.sel'
```

Command-line CVar overrides use the engine's existing argument syntax. Registered console commands use `+commandName` and are queued until the engine has fully initialized and entered its running state. Put command arguments after the command and before the next `+command` or option, according to the command parser implemented in the engine.

Example shape:

```powershell
<path-to-Sandbox.exe> -LogPath Engine.log -EnginePath D:/snow_engine -StartLevel '#engine/Levels/Default.sel' +commandName argument
```

## Runtime development console

- Press tilde/backtick to toggle the ImGui console.
- Use the console to execute registered commands, inspect engine logs, and read or change CVars while the engine is running.
- Startup `+commandName` commands must execute only after full engine initialization, not during static registration or early subsystem startup.
- Commands intended for automation should produce explicit success/failure output in the log and through the local automation bridge.

## MCP automation

The dependency-free MCP server is at
`tools/snowengine_mcp/server.py`. With Sandbox running, launch it with:

```powershell
python tools/snowengine_mcp/server.py --engine-path D:/snow_engine
```

It exposes tools to ping the engine, execute console commands, read or change
CVars, and retrieve logs. The local request/response transport lives under
`.snowengine_mcp/` and is ignored by Git. Do not commit its runtime files.

## Scene conversion workflow

Scene conversion is an explicit, one-time developer action; it must not run automatically during normal scene loading.

1. Download or place source FBX/OpenUSD assets under an intentional content/import location inside the permitted workspace or assigned worktree.
2. Launch Sandbox with the standard engine path and a safe start level.
3. Invoke the registered import command from the runtime console or the local automation bridge.
4. Write converted geometry, scene hierarchy, materials, and texture references to SnowEngine's text-based engine formats.
5. Load the converted level normally on subsequent runs, without requiring the source FBX/OpenUSD file or importer.

Import commands must report actionable errors for missing files, unsupported material features, texture failures, and output-write failures. Avoid committing third-party Bistro source assets unless repository policy explicitly calls for them.

## Common checks

```powershell
git status --short --branch
git diff --check
git diff --stat
rg --files -g '*.sln' -g '*.vcxproj'
```

Before handing off a change:

1. Build `Sandbox.vcxproj` with MSBuild.
2. Launch Sandbox with the standard arguments.
3. Inspect `Engine.log` for assertions, renderer errors, missing assets, and command/import failures.
4. Exercise changed commands through both the ImGui console and the automation bridge when applicable.
5. For scene or renderer changes, load `#engine/Levels/Default.sel` first as a regression check, then load the converted target scene.
6. Report the exact build configuration, launch command, tests performed, and any validation that could not be completed.

## Source and repository hygiene

- Use `rg` and `rg --files` for repository searches.
- Keep generated binaries, logs, imported third-party source archives, and temporary conversion products out of commits unless explicitly required.
- Keep commits focused and describe behavior changes and validation.
- Avoid adding automatic conversion to the normal runtime loading path.
- Favor deterministic, text-based converted output so reviews and future rebuilds are reproducible.
