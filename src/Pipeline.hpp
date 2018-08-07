#include "Pipeline.h"

#include <typeinfo>

#include <map>
#include <string_view>

namespace
{
	template<typename T>
	struct typeid_filler;

	template<>
	struct typeid_filler<std::tuple<>>
	{
		template<class T>
		static void fill_typeids( std::vector<T>& vec ) {}
	};

	template<typename First, typename ...Args>
	struct typeid_filler<std::tuple<First, Args...>>
	{
		template<class T>
		static void fill_typeids( std::vector<T>& vec )
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
			1. make typeid -> input, output, rendernode_ptr
			2. find input resources and fill layers one by one

		not optimal performance-wise, but it doesn't matter, pipeline rebuild occurs very rarely and typical frame graph is rather small
	*/

	auto active_node_info = CollectActiveNodes();
	for ( auto& [node_id, info] : active_node_info )
	{
		boost::sort( info.input_ids );
		boost::sort( info.output_ids );
	}

	auto available_resources = FindInputResources( active_node_info );

	m_node_layers.clear();
	for ( bool layer_created = true; layer_created; )
	{
		layer_created = false;
		std::vector<BaseRenderNode*> new_layer;
		std::set<TypeIdWithName> layer_output_resources;
		for ( auto node_iter = active_node_info.begin(); node_iter != active_node_info.end(); )
		{
			const auto& node_info = node_iter->second;
			if ( boost::range::includes( available_resources, node_info.input_ids ) )
			{
				new_layer.push_back( node_info.node_ptr );
				layer_output_resources.insert( node_info.output_ids.begin(), node_info.output_ids.end() );
				node_iter = active_node_info.erase( node_iter );
			}
			else
			{
				node_iter++;
			}
		}

		if ( ! new_layer.empty() )
		{
			m_node_layers.push_back( std::move( new_layer ) );
			available_resources.merge( layer_output_resources );
			layer_created = true;
		}
	}

	if ( ! active_node_info.empty() )
		throw SnowEngineException( "pipeline can't be created, resource dependency cycle has been found" );

	m_need_to_rebuild_pipeline = false;
}

template<template <typename> class ...Node>
void Pipeline<Node...>::Run()
{
	NOTIMPL;
}

template<template <typename> class ...Node>
std::map<size_t, typename Pipeline<Node...>::RuntimeNodeInfo> Pipeline<Node...>::CollectActiveNodes()
{
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

	return std::move( active_node_info );
}

template<template <typename> class ...Node>
std::set<typename Pipeline<Node...>::TypeIdWithName> Pipeline<Node...>::FindInputResources( const std::map<size_t, RuntimeNodeInfo>& active_nodes )
{
	std::set<TypeIdWithName> input_resources;
	for ( const auto&[node_id, info] : active_nodes )
		for ( const auto& input_res : info.input_ids )
			input_resources.insert( input_res );

	for ( const auto&[node_id, info] : active_nodes )
		for ( const auto& output_res : info.output_ids )
			input_resources.erase( output_res );

	return std::move( input_resources );
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