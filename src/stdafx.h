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

#include "Ptr.h"
#include "RenderUtils.h"

class SnowEngineException
{
public:
	SnowEngineException()
	{}
	SnowEngineException( std::string msg )
		: m_msg( std::move( msg ) )
	{
		MessageBoxA( NULL, "se", m_msg.c_str(), MB_OK );
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