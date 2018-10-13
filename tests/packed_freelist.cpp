#include <boost/test/unit_test.hpp>

#include "../src/utils/packed_freelist.h"

BOOST_AUTO_TEST_SUITE( packed_freelist_tests )

BOOST_AUTO_TEST_CASE( creation )
{
	packed_freelist<int> lst;

	using id = freelist_id;

	id elem_id = lst.insert( 3 );
	id elem2_id = lst.insert( 2 );

	BOOST_TEST( lst[elem_id] == 3 );
	BOOST_TEST( lst[elem2_id] == 2 );
}

BOOST_AUTO_TEST_CASE( lookup_and_modification )
{
	packed_freelist<int> lst;

	using id = freelist_id;

	id id3 = lst.insert( 3 );
	id id2 = lst.insert( 2 );

	BOOST_TEST( lst.has( id3 ) );
	BOOST_TEST( lst.has( id2 ) );

	id id4 = lst.insert( 4 );

	BOOST_TEST( lst.has( id4 ) );
	BOOST_TEST( lst.has( id3 ) );

	lst.erase( id2 );
	BOOST_TEST( !lst.has( id2 ) );
	BOOST_TEST( lst.has( id3 ) );
	BOOST_TEST( lst.has( id4 ) );

	id id5 = lst.insert( 5 );
	BOOST_TEST( lst.has( id5 ) );
	BOOST_TEST( !lst.has( id2 ) );

	int* elem2 = lst.try_get( id2 );
	BOOST_TEST( ! elem2 );
	int* elem3 = lst.try_get( id3 );
	BOOST_TEST( ( elem3 && ( *elem3 == 3 ) ) );

	lst[id4] = 10;
	BOOST_TEST( lst.get( id4 ) == 10 );
}

BOOST_AUTO_TEST_CASE( clear )
{
	packed_freelist<int> lst;

	using id = freelist_id;

	id id3 = lst.insert( 3 );
	id id2 = lst.insert( 2 );
	id id4 = lst.insert( 4 );

	BOOST_TEST( lst.has( id4 ) );
	BOOST_TEST( lst.has( id3 ) );

	lst.clear();
	BOOST_TEST( !lst.has( id2 ) );
	BOOST_TEST( !lst.has( id3 ) );
	BOOST_TEST( !lst.has( id4 ) );

	id id5 = lst.insert( 5 );
	BOOST_TEST( lst.has( id5 ) );
	BOOST_TEST( !lst.has( id2 ) );
	BOOST_TEST( !lst.has( id3 ) );
	BOOST_TEST( !lst.has( id4 ) );
}

BOOST_AUTO_TEST_CASE( emplace )
{
	packed_freelist<std::pair<std::string, int>> lst;

	using id = freelist_id;

	id id3 = lst.emplace( "test", 3 );
	id id2 = lst.emplace( "test again", 2 );

	BOOST_TEST( lst.has( id3 ) );
	BOOST_TEST( lst[id2].first == "test again" );
}


BOOST_AUTO_TEST_SUITE_END()