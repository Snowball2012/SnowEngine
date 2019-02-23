#include "Framegraph.h"

#include <typeinfo>

#include <map>
#include <string_view>

#include <boost/range/algorithm.hpp>

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
Framegraph<Node...>::Framegraph( Framegraph<Node...>&& rhs ) noexcept
{
	m_node_storage = std::move( rhs.m_node_storage );
	m_resources = std::move( rhs.m_resources );
	m_need_to_rebuild_framegraph = true;
}


template<template <typename> class ... Node>
Framegraph<Node...>& Framegraph<Node...>::operator=( Framegraph<Node...>&& rhs ) noexcept
{
	m_node_storage = std::move( rhs.m_node_storage );
	m_resources = std::move( rhs.m_resources );
	m_need_to_rebuild_framegraph = true;

	return *this;
}


template<template <typename> class ... Node>
void Framegraph<Node...>::Rebuild()
{
	/*
		general scheme:
			1. make typeid -> open, write, read, close, rendernode_ptr
			2. sort nodes for every resource
			3. build directed render graph
			4. make layers

		terrible performance-wise, but it doesn't matter, framegraph rebuild occurs very rarely and typical frame graph is rather small
	*/

	auto active_node_info = CollectActiveNodes();
	for ( auto& [node_id, info] : active_node_info )
	{
		boost::sort( info.open_ids );
		boost::sort( info.write_ids );
		boost::sort( info.read_ids );
		boost::sort( info.close_ids );
	}

	auto edges = BuildFrameGraphEdges( active_node_info );

	m_node_layers.clear();
	for ( bool layer_created = true; layer_created; )
	{
		layer_created = false;
		std::vector<BaseNode*> new_layer;

		std::set<size_t> nodes_with_outcoming_edges;
		for ( const auto& edge : edges )
			nodes_with_outcoming_edges.insert( edge.first->node_id );

		std::set<size_t> nodes_to_remove;
		for ( auto&[node_id, info] : active_node_info )
			if ( nodes_with_outcoming_edges.count( node_id ) == 0 )
			{
				nodes_to_remove.insert( node_id );
				new_layer.push_back( info.node_ptr );
			}

		edges.erase( boost::remove_if( edges, [&nodes_to_remove]( const auto& edge )
		{
			return nodes_to_remove.count( edge.first->node_id ) > 0
				|| nodes_to_remove.count( edge.second->node_id ) > 0;
		} ), edges.end() );

		for ( size_t node_hash : nodes_to_remove )
			active_node_info.erase( node_hash );

		if ( ! new_layer.empty() )
		{
			m_node_layers.emplace_back( std::move( new_layer ) );
			layer_created = true;
		}
	}

	if ( ! active_node_info.empty() )
		throw SnowEngineException( "framegraph can't be created, resource dependency cycle has been found" );

	boost::reverse( m_node_layers );

	m_need_to_rebuild_framegraph = false;
}

template<template <typename> class ...Node>
void Framegraph<Node...>::ClearResources()
{
	std::apply( []( auto& ...res ) { ( ( res = std::nullopt ), ... ); }, m_resources );
}

template<template <typename> class ...Node>
void Framegraph<Node...>::Run( ID3D12GraphicsCommandList& cmd_list )
{
	if ( m_need_to_rebuild_framegraph )
		throw SnowEngineException( "framegraph rebuild is needed" );

	for ( auto& layer : m_node_layers )
		for ( auto& node : layer )
			node->Run( *this, cmd_list );
}

template<template <typename> class ...Node>
std::map<size_t, typename Framegraph<Node...>::RuntimeNodeInfo> Framegraph<Node...>::CollectActiveNodes()
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
				typeid_filler<std::tuple<const NodeType*>>::fill_typeids( node_info.open_ids );
				typeid_filler<typename NodeType::OpenRes>::fill_typeids( node_info.open_ids );
				typeid_filler<typename NodeType::WriteRes>::fill_typeids( node_info.write_ids );
				typeid_filler<typename NodeType::ReadRes>::fill_typeids( node_info.read_ids );
				typeid_filler<typename NodeType::CloseRes>::fill_typeids( node_info.close_ids );
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
std::map<typename Framegraph<Node...>::TypeIdWithName,
	     typename Framegraph<Node...>::ResourceUsers> Framegraph<Node...>::FindResourceUsers( const std::map<size_t, RuntimeNodeInfo>& active_nodes )
{
	std::map<TypeIdWithName, ResourceUsers> resource_users;

	for ( const auto&[node_id, info] : active_nodes )
	{
		for ( const auto& res_id : info.open_ids )
			resource_users[res_id].open_nodes.push_back( &info );
		for ( const auto& res_id : info.write_ids )
			resource_users[res_id].write_nodes.push_back( &info );
		for ( const auto& res_id : info.read_ids )
			resource_users[res_id].read_nodes.push_back( &info );
		for ( const auto& res_id : info.close_ids )
			resource_users[res_id].close_nodes.push_back( &info );
	}

	return std::move( resource_users );
}


template<template <typename> class ...Node>
inline std::vector<std::pair<const typename Framegraph<Node...>::RuntimeNodeInfo*,
	                         const typename Framegraph<Node...>::RuntimeNodeInfo*>> Framegraph<Node...>::BuildFrameGraphEdges( const std::map<size_t, RuntimeNodeInfo>& active_nodes )
{
	const auto& resource_users = FindResourceUsers( active_nodes );

	std::vector<std::pair<const RuntimeNodeInfo*, const RuntimeNodeInfo*>> edges;
	for ( const auto&[res_id, nodes] : resource_users )
	{
		for ( const RuntimeNodeInfo* dst_node : nodes.write_nodes )
			for ( const RuntimeNodeInfo* src_node : nodes.open_nodes )
				edges.emplace_back( src_node, dst_node );

		for ( const RuntimeNodeInfo* dst_node : nodes.read_nodes )
			for ( const RuntimeNodeInfo* src_node : boost::join( nodes.open_nodes, nodes.write_nodes ) )
				edges.emplace_back( src_node, dst_node );

		for ( const RuntimeNodeInfo* dst_node : nodes.close_nodes )
			for ( const RuntimeNodeInfo* src_node : boost::join( boost::join( nodes.open_nodes, nodes.write_nodes ), nodes.read_nodes ) )
				edges.emplace_back( src_node, dst_node );
	}
	return std::move( edges );
}


template<template <typename> class ...Node>
template<template <typename> class N>
N<Framegraph<Node...>>* Framegraph<Node...>::GetNode()
{
	return std::get<NStorage<N<Framegraph>>>( m_node_storage ).node.get_ptr();
}
