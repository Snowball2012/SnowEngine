#pragma once

#include <tuple>
#include <optional>

namespace details
{
    template<typename T>
    struct optional_tuple_helper;

    template<typename ...Args>
    struct optional_tuple_helper<std::tuple<Args...>>
    {
        using type = std::tuple<std::optional<Args>...>;
    };
}

template<typename T>
using OptionalTuple = typename details::optional_tuple_helper<T>::type;