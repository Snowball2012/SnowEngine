#include "UniqueTuple.h"

void test()
{
	static_assert( std::is_same_v<UniqueTuple<std::tuple<int, double, int, int, float, double>>, std::tuple<int, double, float>>, "types are not the same" );
}