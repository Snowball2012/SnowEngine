// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <boost/test/unit_test.hpp>

#include "../src/utils/btree.h"

BOOST_AUTO_TEST_SUITE( btree_tests )

BOOST_AUTO_TEST_CASE( creation )
{
	btree_map<int, int, 4> test_btree;
	BOOST_TEST( test_btree.size() == 0 );

    using Cursor = decltype(test_btree)::cursor_t;
    Cursor new_elem_cr = test_btree.insert( 2, 4 );
	BOOST_TEST( test_btree.size() == 1 );

    new_elem_cr = test_btree.insert( 1, 0 );
	BOOST_TEST( test_btree.size() == 2 );
}

BOOST_AUTO_TEST_SUITE_END()