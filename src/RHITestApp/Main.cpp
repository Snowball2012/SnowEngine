#include "StdAfx.h"

#include "RHITestApp.h"


int main(int argc, char** argv)
{
    RHITestApp app;

    try
    {
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        MessageBoxA(NULL, e.what(), "Fatal error", MB_OK);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}