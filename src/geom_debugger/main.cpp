// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "Application.h"

#include <engine/stdafx.h>

#include <Windows.h>

int APIENTRY WinMain( HINSTANCE hinstance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd )
{
#if defined( DEBUG ) | defined( _DEBUG )
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif	
    try
    {
        Application app( hinstance, cmd_line );

        if ( ! app.Initialize() )
            return 1;
        return app.Run();
    }
    catch ( Utils::DxException& e )
    {
        MessageBox( nullptr, e.ToString().c_str(), L"Snow Engine Exception", MB_OK );
        return 0;
    }
	return 0;
}