#pragma once

#include <algorithm>
#include <iostream>
#include <numeric>
#include <string>
#include <filesystem>
#include <optional>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <exception>

#include <boost/range.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/join.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/clamp.hpp>
#include <boost/container/static_vector.hpp>

namespace bc = boost::container;

#include "MathUtils.h"

#include <dxgi1_5.h>
#include <pix.h>
#include <optick.h>

#include "Ptr.h"

#ifndef ThrowIfFailedH
#define ThrowIfFailedH(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw Utils::DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#include "RenderUtils.h"

namespace Utils
{
	class DxException
	{
	public:
		DxException() = default;
		DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

		std::wstring ToString()const;

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
		int LineNumber = -1;
	};
}

inline std::wstring AnsiToWString( const std::string& str )
{
    WCHAR buffer[512];
    MultiByteToWideChar( CP_ACP, 0, str.c_str(), -1, buffer, 512 );
    return std::wstring( buffer );
}

class SnowEngineException : public std::exception
{
public:
    SnowEngineException()
    {}
    SnowEngineException( std::string msg )
        : m_msg( std::move( msg ) )
    {
        MessageBoxA( NULL, "se", m_msg.c_str(), MB_OK );
    }

    virtual char const* what() const
    {
        return m_msg.c_str();
    }

private:
    friend std::ostream& operator<< ( std::ostream& stream, const SnowEngineException& ex );

    std::string m_msg;
};

inline std::ostream& operator<< ( std::ostream& stream, const SnowEngineException& ex )
{
    stream << ex.m_msg;
    return stream;
}

#ifndef NOTIMPL
#define NOTIMPL throw SnowEngineException( "not implemented yet" );
#endif