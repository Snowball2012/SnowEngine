#!/usr/bin/env python3
"""SnowEngine MCP server.

The server speaks MCP over stdio and exchanges atomic JSON files with a running
SnowEngine process. It has no third-party Python dependencies.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import time
import uuid
from typing import Any


TOOLS: list[dict[str, Any]] = [
    {
        "name": "ping",
        "description": "Check whether SnowEngine is running and processing automation requests.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    {
        "name": "execute_console_command",
        "description": "Execute a registered SnowEngine console command on the engine main thread.",
        "inputSchema": {
            "type": "object",
            "properties": {"command": {"type": "string"}},
            "required": ["command"],
            "additionalProperties": False,
        },
    },
    {
        "name": "get_cvar",
        "description": "Read the current value of a registered SnowEngine CVar.",
        "inputSchema": {
            "type": "object",
            "properties": {"name": {"type": "string"}},
            "required": ["name"],
            "additionalProperties": False,
        },
    },
    {
        "name": "set_cvar",
        "description": "Change the value of a registered SnowEngine CVar at runtime.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "value": {"type": ["string", "number", "boolean"]},
            },
            "required": ["name", "value"],
            "additionalProperties": False,
        },
    },
    {
        "name": "get_logs",
        "description": "Read captured engine log and console lines from an optional line cursor.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "since": {"type": "integer", "minimum": 0, "default": 0},
            },
            "additionalProperties": False,
        },
    },
]


class EngineBridge:
    def __init__(self, engine_path: Path, timeout: float) -> None:
        bridge_path = engine_path.resolve() / ".snowengine_mcp"
        self.requests_path = bridge_path / "requests"
        self.responses_path = bridge_path / "responses"
        self.timeout = timeout
        self.requests_path.mkdir(parents=True, exist_ok=True)
        self.responses_path.mkdir(parents=True, exist_ok=True)

    def call(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        request_id = str(uuid.uuid4())
        request_path = self.requests_path / f"{request_id}.json"
        temp_path = self.requests_path / f"{request_id}.json.tmp"
        response_path = self.responses_path / f"{request_id}.json"

        request = {"id": request_id, "method": method, "params": params}
        with temp_path.open("w", encoding="utf-8", newline="\n") as stream:
            json.dump(request, stream, ensure_ascii=False)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_path, request_path)

        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            if response_path.exists():
                try:
                    with response_path.open("r", encoding="utf-8") as stream:
                        response = json.load(stream)
                    response_path.unlink(missing_ok=True)
                    return response
                except (OSError, json.JSONDecodeError):
                    # Atomic engine writes make this unlikely, but retry until timeout.
                    pass
            time.sleep(0.025)

        request_path.unlink(missing_ok=True)
        raise TimeoutError(
            f"SnowEngine did not answer within {self.timeout:g}s. "
            "Start Sandbox with -EnginePath pointing at this repository."
        )


def write_message(message: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(message, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def error_response(request_id: Any, code: int, message: str) -> dict[str, Any]:
    return {
        "jsonrpc": "2.0",
        "id": request_id,
        "error": {"code": code, "message": message},
    }


def handle_request(bridge: EngineBridge, request: dict[str, Any]) -> dict[str, Any] | None:
    method = request.get("method")
    request_id = request.get("id")

    if request_id is None:
        return None

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "snowengine", "version": "0.1.0"},
            },
        }

    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": request_id, "result": {"tools": TOOLS}}

    if method != "tools/call":
        return error_response(request_id, -32601, f"Method not found: {method}")

    params = request.get("params")
    if not isinstance(params, dict):
        return error_response(request_id, -32602, "tools/call params must be an object")
    tool_name = params.get("name")
    arguments = params.get("arguments", {})
    if not isinstance(arguments, dict):
        return error_response(request_id, -32602, "Tool arguments must be an object")
    if tool_name not in {tool["name"] for tool in TOOLS}:
        return error_response(request_id, -32602, f"Unknown tool: {tool_name}")

    if tool_name == "set_cvar" and "value" in arguments:
        value = arguments["value"]
        if isinstance(value, bool):
            arguments["value"] = "true" if value else "false"
        else:
            arguments["value"] = str(value)

    try:
        engine_response = bridge.call(str(tool_name), arguments)
        ok = bool(engine_response.get("ok"))
        text = json.dumps(engine_response, ensure_ascii=False, indent=2)
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {
                "content": [{"type": "text", "text": text}],
                "isError": not ok,
            },
        }
    except (OSError, TimeoutError) as error:
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {
                "content": [{"type": "text", "text": str(error)}],
                "isError": True,
            },
        }


def main() -> int:
    parser = argparse.ArgumentParser(description="SnowEngine MCP server")
    parser.add_argument(
        "--engine-path",
        type=Path,
        default=Path(os.environ.get("SNOWENGINE_PATH", Path.cwd())),
        help="SnowEngine repository root (defaults to SNOWENGINE_PATH or cwd)",
    )
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    bridge = EngineBridge(args.engine_path, args.timeout)
    for line in sys.stdin:
        try:
            request = json.loads(line)
            if not isinstance(request, dict):
                raise ValueError("JSON-RPC request must be an object")
            response = handle_request(bridge, request)
            if response is not None:
                write_message(response)
        except (json.JSONDecodeError, ValueError) as error:
            write_message(error_response(None, -32700, str(error)))
        except Exception as error:  # Keep the MCP process alive and report failures.
            write_message(error_response(None, -32603, str(error)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
