#pragma once

#include "utils/OptionalTuple.h"
#include "utils/UniqueTuple.h"

#include <map>
#include <set>
#include <string_view>
#include <typeinfo>

#include <boost/range/algorithm.hpp>

template<typename Resource, D3D12_RESOURCE_STATES _state, typename Enabler>
struct ResourceInState
{
	using resource = Resource;
	static constexpr D3D12_RESOURCE_STATES state = _state;
};

namespace details
{
	// basic reflection stuff
	struct TypeIdWithName
	{
		size_t id;
		std::string_view name; // name is stored purely for debug purposes

		bool operator==( const TypeIdWithName& other ) const { return this->id == other.id; }
		bool operator<( const TypeIdWithName& other ) const { return this->id < other.id; }
	};


	// tuple -> vector of TypeIdWithName
	template<typename T>
	struct TypeidFiller;

	template<>
	struct TypeidFiller<std::tuple<>>
	{
		static void FillTypeids( std::vector<TypeIdWithName>& vec ) {}
	};

	template<typename First, typename ...Rest>
	struct TypeidFiller<std::tuple<First, Rest...>>
	{
		static void FillTypeids( std::vector<TypeIdWithName>& vec )
		{
			vec.push_back( TypeIdWithName{ typeid( First ).hash_code(), typeid( Rest ).name() } );

			using RestOfTuple = std::tuple<Rest...>;
			TypeidFiller<RestOfTuple>::FillTypeids( vec );
		}
	};


	namespace framegraph
	{
		// node helpers
		template<typename Node>
		struct OptionalFgNode
		{
			std::optional<Node> node = std::nullopt;
			bool enabled = false;
		};

		template<typename Framegraph>
		using BaseNode = BaseRenderNode<Framegraph>;

		template<typename Framegraph>
		struct RuntimeNodeInfo
		{
			size_t node_id;
			std::string_view node_name; // debug info
			BaseNode<Framegraph>* node_ptr;
			std::vector<TypeIdWithName> open_ids;
			std::vector<TypeIdWithName> write_ids;
			std::vector<TypeIdWithName> read_ids;
			std::vector<TypeIdWithName> close_ids;
		};


		// resource helpers

		// extracts resource from TrackedResource
		template<typename T>
		struct StrippedResource
		{
			using Type = T;
		};

		template<typename Resource, D3D12_RESOURCE_STATES state>
		struct StrippedResource<ResourceInState<Resource, state>>
		{
			using Type = typename ResourceInState<Resource, state>::resource;
		};


		// tuple<TrackedResource> -> tuple<StrippedResource<TrackedResource>>
		template<typename T>
		struct StrippedResourceTuple;

		template<typename ... Args>
		struct StrippedResourceTuple<std::tuple<Args...>>
		{
			using Type = std::tuple<typename StrippedResource<Args>::Type ...>;
		};


		// Node -> tuple of every resource Node uses
		template<typename Node>
		using NodeResources = UniqueTuple<decltype( std::tuple_cat( std::declval<typename Node::OpenRes>(),
																	std::declval<typename Node::WriteRes>(),
																	std::declval<typename Node::ReadRes>(),
																	std::declval<typename Node::CloseRes>(),
																	std::tuple<const Node*>() ) )>;

		template<typename Framegraph>
		struct ResourceUsers
		{
			using Info = RuntimeNodeInfo<Framegraph>;

			std::vector<const Info*> open_nodes;
			std::vector<const Info*> write_nodes;
			std::vector<const Info*> read_nodes;
			std::vector<const Info*> close_nodes;
		};



		template<template <typename> class ... Nodes>
		class FramegraphImpl
		{
			using FramegraphInstance = FramegraphImpl<Nodes...>;

		public:
			FramegraphImpl() noexcept = default;

			FramegraphImpl( FramegraphImpl&& rhs ) noexcept;
			FramegraphImpl& operator=( FramegraphImpl&& rhs ) noexcept;

			FramegraphImpl( const FramegraphImpl& ) = delete;
			FramegraphImpl& operator=( const FramegraphImpl& ) = delete;


			template<template <typename> class Node, typename ... Args>
			void ConstructNode( Args&& ... args );

			template<template <typename> class Node>
			void Enable();

			template<template <typename> class Node>
			void Disable();

			template<template <typename> class Node>
			Node<FramegraphInstance>* GetNode();

			template<typename Res>
			std::optional<Res>& GetRes()
			{
				return std::get<std::optional<Res>>( m_resources );
			}

			template<typename Res>
			void SetRes( const Res& res )
			{
				std::get<std::optional<Res>>( m_resources ) = res;
			}

			void Rebuild();
			void ClearResources();

			void Run( ID3D12GraphicsCommandList& cmd_list );

			bool IsRebuildNeeded() const
			{
				return m_need_to_rebuild_framegraph;
			}

		private:

			using FramegraphResources = UniqueTuple<
				typename StrippedResourceTuple<
				    decltype( std::tuple_cat( std::declval<NodeResources<Nodes<FramegraphInstance>>>()... ) )
				>::Type
			>;

			using ResourceUsers = framegraph::ResourceUsers<FramegraphInstance>;
			using RuntimeNodeInfo = framegraph::RuntimeNodeInfo<FramegraphInstance>;
			using BaseNode = framegraph::BaseNode<FramegraphInstance>;

			using NodeStorage = std::tuple<OptionalFgNode<Nodes<FramegraphInstance>>...>;

			NodeStorage m_node_storage;
			OptionalTuple<FramegraphResources> m_resources;
			std::vector<std::vector<BaseNode*>> m_node_layers; // runtime framegraph
			bool m_need_to_rebuild_framegraph = true;

			std::map<size_t, RuntimeNodeInfo> CollectActiveNodes();
			std::map<TypeIdWithName, ResourceUsers> FindResourceUsers( const std::map<size_t, RuntimeNodeInfo>& active_nodes );
			std::vector<std::pair<const RuntimeNodeInfo*, const RuntimeNodeInfo*>> BuildFrameGraphEdges( const std::map<size_t, RuntimeNodeInfo>& active_nodes );
		};


		template<template <typename> class ... Nodes>
		FramegraphImpl<Nodes...>::FramegraphImpl( FramegraphImpl<Nodes...>&& rhs ) noexcept
		{
			m_node_storage = std::move( rhs.m_node_storage );
			m_resources = std::move( rhs.m_resources );
			m_need_to_rebuild_framegraph = true;
		}


		template<template <typename> class ... Nodes>
		FramegraphImpl<Nodes...>& FramegraphImpl<Nodes...>::operator=( FramegraphImpl<Nodes...>&& rhs ) noexcept
		{
			m_node_storage = std::move( rhs.m_node_storage );
			m_resources = std::move( rhs.m_resources );
			m_need_to_rebuild_framegraph = true;

			return *this;
		}


		template<template <typename> class ... Nodes>
		void FramegraphImpl<Nodes...>::Rebuild()
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
			for ( auto&[node_id, info] : active_node_info )
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

		template<template <typename> class ...Nodes>
		void FramegraphImpl<Nodes...>::ClearResources()
		{
			std::apply( []( auto& ...res ) { ( ( res = std::nullopt ), ... ); }, m_resources );
		}

		template<template <typename> class ...Nodes>
		void FramegraphImpl<Nodes...>::Run( ID3D12GraphicsCommandList& cmd_list )
		{
			if ( m_need_to_rebuild_framegraph )
				throw SnowEngineException( "framegraph rebuild is needed" );

			for ( auto& layer : m_node_layers )
				for ( auto& node : layer )
					node->Run( *this, cmd_list );
		}

		template<template <typename> class ...Nodes>
		std::map<size_t, typename FramegraphImpl<Nodes...>::RuntimeNodeInfo> FramegraphImpl<Nodes...>::CollectActiveNodes()
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
						TypeidFiller<std::tuple<const NodeType*>>::FillTypeids( node_info.open_ids );

						TypeidFiller<typename StrippedResourceTuple<typename NodeType::OpenRes>::Type>::FillTypeids( node_info.open_ids );
						TypeidFiller<typename StrippedResourceTuple<typename NodeType::WriteRes>::Type>::FillTypeids( node_info.write_ids );
						TypeidFiller<typename StrippedResourceTuple<typename NodeType::ReadRes>::Type>::FillTypeids( node_info.read_ids );
						TypeidFiller<typename StrippedResourceTuple<typename NodeType::CloseRes>::Type>::FillTypeids( node_info.close_ids );
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

		template<template <typename> class ...Nodes>
		std::map<TypeIdWithName,
			typename FramegraphImpl<Nodes...>::ResourceUsers> FramegraphImpl<Nodes...>::FindResourceUsers( const std::map<size_t, RuntimeNodeInfo>& active_nodes )
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


		template<template <typename> class ...Nodes>
		inline std::vector<std::pair<const typename FramegraphImpl<Nodes...>::RuntimeNodeInfo*,
			const typename FramegraphImpl<Nodes...>::RuntimeNodeInfo*>> FramegraphImpl<Nodes...>::BuildFrameGraphEdges( const std::map<size_t, RuntimeNodeInfo>& active_nodes )
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


		template<template <typename> class ... Nodes>
		template<template <typename> class Node, typename ... Args>
		void FramegraphImpl<Nodes...>::ConstructNode( Args && ...args )
		{

			auto& node_storage = std::get<OptionalFgNode<Node<FramegraphImpl>>>( m_node_storage );
			node_storage.node.emplace( std::forward<Args>( args )... );
		}


		template<template <typename> class ... Nodes>
		template<template <typename> class Node>
		void FramegraphImpl<Nodes...>::Enable()
		{
			auto& node_storage = std::get<OptionalFgNode<Node<FramegraphImpl>>>( m_node_storage );

			if ( ! node_storage.node.has_value() )
				throw SnowEngineException( "node is not constructed yet" );
			node_storage.enabled = true;

			m_need_to_rebuild_framegraph = true;
		}


		template<template <typename> class ... Nodes>
		template<template <typename> class Node>
		void FramegraphImpl<Nodes...>::Disable()
		{
			auto& node_storage = std::get<OptionalFgNode<Node<FramegraphImpl>>>( m_node_storage );

			if ( ! node_storage.node.has_value() )
				throw SnowEngineException( "node is not constructed yet" );

			node_storage.enabled = false;

			m_need_to_rebuild_framegraph = true;
		}

		template<template <typename> class ...Nodes>
		template<template <typename> class N>
		N<FramegraphImpl<Nodes...>>* FramegraphImpl<Nodes...>::GetNode()
		{
			return std::get<OptionalFgNode<N<FramegraphImpl>>>( m_node_storage ).node.get_ptr();
		}

	}
}