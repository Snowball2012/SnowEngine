// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UniqueTuple.h"
#include "OptionalTuple.h"

void test_unique()
{
	static_assert( std::is_same_v<UniqueTuple<std::tuple<int, double, int, int, float, double>>, std::tuple<int, double, float>>, "types are not the same" );
}

void test_optional()
{
	static_assert( std::is_same_v<OptionalTuple<std::tuple<int, double>>,
				                  std::tuple<std::optional<int>, std::optional<double>>> , "types are not the same" );
}