// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <boost/test/unit_test.hpp>

#include "../src/utils/btree.h"

#include <random>

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

    std::vector<char> deleted( num_keys, 1 );

    auto callback = [&]( int key, const auto& new_cursor )
    {
        if ( new_cursor.key() != key || new_cursor.value() != random_numbers[key] )
            bool wrk = true;
        cursors[key] = &new_cursor.node->values()[new_cursor.position];
    };

	btree_map<int, int, 4, std::allocator<btree_map_node<int, int, 4>>, decltype(callback)> test_btree( callback );
    for ( int i = 0; i < random_numbers.size(); ++i )
    {
        auto cursor = test_btree.insert( i, random_numbers[i] );
        deleted[i] = 0;
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

    while ( test_btree.size() > num_keys / 2 )
    {
        auto i = test_btree.begin();

        int rand_elem = rand() % test_btree.size();
        for ( int j = 0; j < rand_elem; j++ )
            i = test_btree.get_next( i );

        deleted[i.key()] = 1;
        test_btree.erase( i );
    }

    for ( int i = 0; i < random_numbers.size(); ++i )
        if ( ! deleted[i] )
            BOOST_TEST( *cursors[i] == random_numbers[i] );

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
    size_t total_size = test_btree.size();
    size_t deleted_elems = 0;
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

    size_t total_size = test_btree.size();
    size_t deleted_elems = 0;
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

BOOST_AUTO_TEST_CASE( stress_test )
{
	constexpr int TEST_SIZE = 50000;

	struct useful
	{
		int id;
		useful( int new_id = 0 ) : id( new_id ) { }
	};

	std::vector<useful*> g_vector;
	
	btree_map<useful*, int, 30> g_btree;
	
	useful g_pool[TEST_SIZE];
	for ( int i = 0; i < TEST_SIZE; ++i )
		g_pool[i].id = i + 1;

	std::vector<int> insert_order;
	insert_order.resize(TEST_SIZE);
	for ( int i = 0; i < TEST_SIZE; i++ )
		insert_order[i] = i;
	
	std::random_device rd;
	std::mt19937 g( rd() );
 
	std::shuffle(insert_order.begin(), insert_order.end(), g);
	for ( int i = 0; i < TEST_SIZE; ++i )
	{
		g_btree.insert( g_pool + insert_order[i], 0 );
	}

	
	uint64_t sum = 0;
	g_btree.for_each( [&sum]( const auto& key, const auto& v )
	{
		uint64_t val;
		val = reinterpret_cast<uint64_t>( key );
		sum++;
		sum *= ( val * val + val ) % 15 * ( val - 1 ) + 1;
	} );

	for ( int i = 0; i < TEST_SIZE; ++i )
	{
		auto cursor = g_btree.find( g_pool + i );
		g_btree.erase( cursor );
	}

	BOOST_TEST( g_btree.size() == 0 );
}

BOOST_AUTO_TEST_SUITE_END()