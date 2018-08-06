#include "Pipeline.h"

#include <typeinfo>

#include <map>
#include <string_view>

namespace
{
	struct TypeIdWithName
	{
		size_t id;
		std::string_view name; // name is stored purely for debug purposes
	};

	struct RuntimeNodeInfo
	{
		size_t node_id;
		std::string_view node_name; // debug info
		BaseRenderNode* node_ptr;
		std::vector<TypeIdWithName> input_ids;
		std::vector<TypeIdWithName> output_ids;
	};

	template<typename T>
	struct typeid_filler;

	template<>
	struct typeid_filler<std::tuple<>>
	{
		static void fill_typeids( std::vector<TypeIdWithName>& vec ) {}
	};

	template<typename First, typename ...Args>
	struct typeid_filler<std::tuple<First, Args...>>
	{
		static void fill_typeids( std::vector<TypeIdWithName>& vec )
		{
			vec.push_back( { typeid( First ).hash_code(), typeid( First ).name() } );
			typeid_filler<std::tuple<Args...>>::fill_typeids( vec );
		}
	};
	
}

template<template <typename> class ... Node>
void Pipeline<Node...>::RebuildPipeline()
{
	/*
		general scheme:
			make typeid -> input, output, rendernode_ptr
	*/

	std::map<size_t, RuntimeNodeInfo> active_node_info;

	auto fill_info = [&active_node_info]( auto&& ...nodes )
	{
		auto f_impl = [&active_node_info]( auto& self, auto& node, auto&& ... rest )
		{
			if ( node.node.has_value() && node.enabled )
			{
				using NodeType = std::decay_t<decltype( *node.node )>;
				const size_t node_id = typeid( NodeType ).hash_code();
				RuntimeNodeInfo& node_info = active_node_info.emplace( node_id, RuntimeNodeInfo() ).first->second;
				node_info.node_id = node_id;
				node_info.node_ptr = &*node.node;
				node_info.node_name = typeid( NodeType ).name();
				typeid_filler<typename NodeType::InputResources>::fill_typeids( node_info.input_ids );
				typeid_filler<typename NodeType::OutputResources>::fill_typeids( node_info.output_ids );
			}
			if constexpr ( sizeof...( rest ) > 0 )
				self( self, rest... );
			return;
		};
		f_impl( f_impl, nodes... );
		return;
	};

	std::apply( fill_info, m_node_storage );

	NOTIMPL;
	m_need_to_rebuild_pipeline = false;
}

template<template <typename> class ...Node>
inline void Pipeline<Node...>::Run()
{
	NOTIMPL;
}

template<template <typename> class ...Node>
template<template <typename> class N>
N<Pipeline<Node...>>* Pipeline<Node...>::GetNode()
{
	return std::get<NStorage<N<Pipeline>>>( m_node_storage ).node.get_ptr();
}

namespace Testing
{
	void create_pipeline();
}