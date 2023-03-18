#include "StdAfx.h"

#include "SandboxApp.h"


using AppClass = SandboxApp;

int main( int argc, char** argv )
{
    AppClass app;

    try
    {
        app.Run( argc, argv );
    }
    catch ( const std::exception& e )
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        MessageBoxA( NULL, e.what(), "Fatal error", MB_OK );
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}