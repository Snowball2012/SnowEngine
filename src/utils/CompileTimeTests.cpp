// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com
#include "UniqueTuple.h"

void test()
{
	static_assert( std::is_same_v<UniqueTuple<std::tuple<int, double, int, int, float, double>>, std::tuple<int, double, float>>, "types are not the same" );
}
