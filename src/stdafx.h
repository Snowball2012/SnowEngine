#pragma once

#include <string>
#include <unordered_map>

#include <boost/optional.hpp>
#include <boost/range.hpp>
#include <boost/range/algorithm.hpp>

#include <wrl.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class SnowEngineException
{
public:
	SnowEngineException()
	{}
	SnowEngineException( std::string msg )
		: m_msg( std::move( msg ) )
	{}

private:
	friend std::ostream& operator<< ( std::ostream& stream, const SnowEngineException& ex );

	std::string m_msg;
};

std::ostream& operator<< ( std::ostream& stream, const SnowEngineException& ex )
{
	stream << ex.m_msg;
	return stream;
}