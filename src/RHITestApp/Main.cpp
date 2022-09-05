#include "StdAfx.h"

#include "RHITestApp.h"

#include "D3D12TestApp.h"


#define TEST_D3D12

#ifdef TEST_D3D12
using AppClass = D3D12TestApp;
#else
using AppClass = RHITestApp;
#endif

int main(int argc, char** argv)
{
    AppClass app;

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