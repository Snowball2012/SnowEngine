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


BOOST_AUTO_TEST_CASE( callback )
{
    constexpr int num_keys = 100;
    constexpr int num_samples = 200;

    std::vector<int> random_numbers;
    random_numbers.reserve( num_keys );
    for ( int i = 0; i < num_keys; ++i )
        random_numbers.push_back( rand() );
    
    std::vector<int*> cursors;
    cursors.resize(num_keys);

    auto callback = [&cursors]( int key, const auto& new_cursor )
    {
        cursors[key] = new_cursor.node->values() + new_cursor.position;
    };

	btree_map<int, int, 4, std::allocator<btree_map_node<int, int, 4>>, decltype(callback)> test_btree( callback );
    for ( int i = 0; i < random_numbers.size(); ++i )
    {
        auto cursor = test_btree.insert( i, random_numbers[i] );
        cursors[i] = cursor.node->values() + cursor.position;
    }

    BOOST_TEST( test_btree.size() == num_keys );

    for ( int i = 0; i < num_samples; ++i )
    {
        int key = rand() % random_numbers.size();
        
        BOOST_TEST( *cursors[key] == random_numbers[key] );
    }

    for ( int i = 0; i < num_keys; ++i )
    {
        BOOST_TEST( *cursors[i] == random_numbers[i] );
    }
}


BOOST_AUTO_TEST_CASE( iteration )
{
    constexpr int num_keys = 100;

    std::vector<int> random_numbers;
    random_numbers.reserve( num_keys );
    for ( int i = 0; i < num_keys; ++i )
        random_numbers.push_back( rand() );
    
    std::vector<int*> cursors;
    cursors.resize(num_keys);

    auto callback = [&cursors]( int key, const auto& new_cursor )
    {
        cursors[key] = new_cursor.node->values() + new_cursor.position;
    };

	btree_map<int, int, 4, std::allocator<btree_map_node<int, int, 4>>, decltype(callback)> test_btree( callback );
    for ( int i = 0; i < random_numbers.size(); ++i )
    {
        auto cursor = test_btree.insert( i, random_numbers[i] );
        cursors[i] = cursor.node->values() + cursor.position;
    }

    BOOST_TEST( test_btree.size() == num_keys );

    auto i = test_btree.begin();
    int prev_key = i.node->keys()[i.position];
    i = test_btree.get_next( i );
    size_t nelems = 1;
    for ( ; i != test_btree.end(); i = test_btree.get_next( i ), nelems++ )
    {
        BOOST_TEST( i.node );
        BOOST_TEST( i.position < i.node->num_elems );
        int key = i.node->keys()[i.position];
        BOOST_TEST( key > prev_key );
        prev_key = key;
    }

    BOOST_TEST( nelems == test_btree.size() );
}


BOOST_AUTO_TEST_CASE( sequenced_deletion )
{
    constexpr int num_keys = 100;

    std::vector<int> random_numbers;
    random_numbers.reserve( num_keys );
    for ( int i = 0; i < num_keys; ++i )
        random_numbers.push_back( rand() );
    
    std::vector<int*> cursors;
    cursors.resize(num_keys);

    auto callback = [&cursors]( int key, const auto& new_cursor )
    {
        cursors[key] = new_cursor.node->values() + new_cursor.position;
    };

	btree_map<int, int, 4, std::allocator<btree_map_node<int, int, 4>>, decltype(callback)> test_btree( callback );
    for ( int i = 0; i < random_numbers.size(); ++i )
    {
        auto cursor = test_btree.insert( i, random_numbers[i] );
        cursors[i] = cursor.node->values() + cursor.position;
    }

    BOOST_TEST( test_btree.size() == num_keys );

    auto i = test_btree.begin();
    int total_size = test_btree.size();
    int deleted_elems = 0;
    while ( i != test_btree.end() )
    {
        i = test_btree.erase( i );
        deleted_elems++;
    }

    BOOST_TEST( total_size == deleted_elems );
    BOOST_TEST( (test_btree.begin() == test_btree.end()) );
}


BOOST_AUTO_TEST_CASE( randomized_deletion )
{
    constexpr int num_keys = 100;

    std::vector<int> random_numbers;
    random_numbers.reserve( num_keys );
    for ( int i = 0; i < num_keys; ++i )
        random_numbers.push_back( rand() );
    
    std::vector<int*> cursors;
    cursors.resize(num_keys);

    auto callback = [&cursors]( int key, const auto& new_cursor )
    {
        cursors[key] = new_cursor.node->values() + new_cursor.position;
    };

	btree_map<int, int, 4, std::allocator<btree_map_node<int, int, 4>>, decltype(callback)> test_btree( callback );
    for ( int i = 0; i < random_numbers.size(); ++i )
    {
        auto cursor = test_btree.insert( i, random_numbers[i] );
        cursors[i] = cursor.node->values() + cursor.position;
    }

    BOOST_TEST( test_btree.size() == num_keys );

    int total_size = test_btree.size();
    int deleted_elems = 0;
    while ( test_btree.size() )
    {
        auto i = test_btree.begin();

        int rand_elem = rand() % test_btree.size();
        for ( int j = 0; j < rand_elem; j++ )
            i = test_btree.get_next( i );

        test_btree.erase( i );
        deleted_elems++;
    }

    BOOST_TEST( total_size == deleted_elems );
    BOOST_TEST( (test_btree.begin() == test_btree.end()) );
}

BOOST_AUTO_TEST_SUITE_END()