#include <tuple>
#include <type_traits>

#pragma once

namespace details
{
	template <typename T, typename Tuple>
	struct has_type;

	template <typename T>
	struct has_type<T, std::tuple<>> : std::false_type {};

	template <typename T, typename U, typename... Ts>
	struct has_type<T, std::tuple<U, Ts...>> : has_type<T, std::tuple<Ts...>> {};

	template <typename T, typename... Ts>
	struct has_type<T, std::tuple<T, Ts...>> : std::true_type {};


	template <typename T1, typename T2>
	struct unique_tuple_helper;
	template <typename T1, typename T2, bool b>
	struct unique_tuple_deducer;

	template <typename UniqueTuple, typename TupleArg, typename ... Args>
	struct unique_tuple_deducer<UniqueTuple, std::tuple<TupleArg, Args...>, false>
		: public unique_tuple_helper<decltype( std::tuple_cat( std::declval<UniqueTuple>(), std::declval<std::tuple<TupleArg>>() ) ),
		std::tuple<Args...>>
	{};

	template <typename UniqueTuple, typename TupleArg, typename ... Args>
	struct unique_tuple_deducer<UniqueTuple, std::tuple<TupleArg, Args...>, true>
		: public unique_tuple_helper<UniqueTuple, std::tuple<Args...>>
	{};

	template <typename UniqueTuple>
	struct unique_tuple_helper<UniqueTuple, std::tuple<>>
	{
		using type = UniqueTuple;
	};

	template <typename UniqueTuple, typename TupleArg, typename ... Args>
	struct unique_tuple_helper<UniqueTuple, std::tuple<TupleArg, Args...>>
		: public unique_tuple_deducer<UniqueTuple, std::tuple<TupleArg, Args...>, has_type<TupleArg, UniqueTuple>::value>
	{};
}

template <typename T>
using UniqueTuple = typename details::unique_tuple_helper<std::tuple<>, T>::type;

namespace details
{
	
}