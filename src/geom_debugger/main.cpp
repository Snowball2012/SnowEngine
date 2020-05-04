// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"
#include <Windows.h>

int APIENTRY WinMain( HINSTANCE hinstance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd )
{
#if defined( DEBUG ) | defined( _DEBUG )
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    MessageBox( nullptr, L"Hello, World!", L"Geometry Debugger", MB_OK );
	return 0;
}