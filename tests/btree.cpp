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

    test_btree.clear();
	BOOST_TEST( test_btree.size() == 0 );
}

BOOST_AUTO_TEST_CASE( search )
{
    constexpr int num_keys = 100;
    constexpr int num_samples = 200;

    std::vector<int> random_numbers;
    random_numbers.reserve( num_keys );
    for ( int i = 0; i < num_keys; ++i )
        random_numbers.push_back( rand() );
    
	btree_map<int, int, 4> test_btree;
    for ( int i = 0; i < random_numbers.size(); ++i )
        test_btree.insert( i, random_numbers[i] );

    BOOST_TEST( test_btree.size() == num_keys );

    for ( int i = 0; i < num_samples; ++i )
    {
        int key = rand() % random_numbers.size();
        auto res = test_btree.find( key );

        BOOST_TEST_REQUIRE( res.node != nullptr, ( std::string( "key is " ) + std::to_string( key ) ).c_str() );
        BOOST_TEST( res.node->keys()[res.position] == key );
        BOOST_TEST( res.node->values()[res.position] == random_numbers[key] );
    }
}

BOOST_AUTO_TEST_SUITE_END()