// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "RenderUtils.h"

#include <d3dx12.h>
#include <d3dcompiler.h>

#include <comdef.h>

#include <fstream>

namespace Utils
{
    ComPtr<ID3DBlob> LoadBinary( const std::wstring& filename )
    {
        std::ifstream fin( filename, std::ios::binary );

        fin.seekg( 0, std::ios_base::end );
        std::ifstream::pos_type size = (int)fin.tellg();
        fin.seekg( 0, std::ios_base::beg );

        ComPtr<ID3DBlob> blob;
        ThrowIfFailedH( D3DCreateBlob( size, blob.GetAddressOf() ) );

        fin.read( (char*)blob->GetBufferPointer(), size );
        fin.close();

        return blob;
    }

    DxException::DxException( HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber ):
        ErrorCode( hr ),
        FunctionName( functionName ),
        Filename( filename ),
        LineNumber( lineNumber )
    {
        MessageBoxW( NULL, ToString().c_str(), L"exception", MB_OK );
    }

    std::wstring DxException::ToString()const
    {
        // Get the string description of the error code.
        _com_error err( ErrorCode );
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring( LineNumber ) + L"; error: " + msg;
    }
}
