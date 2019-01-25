#pragma once

#include "utils/OptionalTuple.h"
#include "utils/UniqueTuple.h"

#include <map>
#include <set>

// generic cpu-gpu rendering pipeline
// TODO: 2 different pipelines ( cpu and gpu ) for cpu resources and gpu resources instead of fused one

struct ID3D12GraphicsCommandList;

class BaseRenderNode
{
public:
	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) = 0;
};

template<template <typename> class ... Node>
class Pipeline
{
public:
	template<template <typename> class N, typename ... Args>
	void ConstructNode( Args&& ... args )
	{
		auto& node_storage = std::get<NStorage<N<Pipeline>>>( m_node_storage );
		node_storage.node.emplace( this, std::forward<Args>( args )... );
	}

	template<template <typename> class N>
	void Enable()
	{
		auto& node_storage = std::get<NStorage<N<Pipeline>>>( m_node_storage );

		if ( ! node_storage.node.has_value() )
			throw SnowEngineException( "node is not constructed yet" );
		node_storage.enabled = true;

		m_need_to_rebuild_pipeline = true;
	}

	template<template <typename> class N, typename ... Args>
	void ConstructAndEnableNode( Args&& ... args )
	{
		ConstructNode<N>( args... );
		Enable<N>();
	}	

	template<template <typename> class N>
	void Disable()
	{
		auto& node_storage = std::get<NStorage<N<Pipeline>>>( m_node_storage );

		if ( ! node_storage.node.has_value() )
			throw SnowEngineException( "node is not constructed yet" );

		node_storage.enabled = false;

		m_need_to_rebuild_pipeline = true;
	}

	template<template <typename> class N>
	N<Pipeline>* GetNode();

	template<typename Res>
	std::optional<Res>& GetRes( )
	{
		return std::get<std::optional<Res>>( m_resources );
	}

	template<typename Res>
	void SetRes( const Res& res )
	{
		std::get<std::optional<Res>>( m_resources ) = res;
	}

	void RebuildPipeline();
	void Run( ID3D12GraphicsCommandList& cmd_list );

	bool IsRebuildNeeded() const
	{
		return m_need_to_rebuild_pipeline;
	}

private:

	// data
	template<typename N>
	struct NStorage
	{
		std::optional<N> node = std::nullopt;
		bool enabled = false;
	};

	std::tuple<NStorage<Node<Pipeline>>...> m_node_storage;

	bool m_need_to_rebuild_pipeline = true;

	template<typename Node>
	using NodeResources = UniqueTuple<decltype( std::tuple_cat( std::declval<typename Node::OpenRes>(),
																std::declval<typename Node::WriteRes>(),
																std::declval<typename Node::ReadRes>(),
																std::declval<typename Node::CloseRes>(),
																std::tuple<const Node*>() ) )>;

	using PipelineResources = UniqueTuple<decltype( std::tuple_cat( std::declval<NodeResources<Node<Pipeline>>>()... ) )>;

	OptionalTuple<PipelineResources> m_resources;

	std::vector<std::vector<BaseRenderNode*>> m_node_layers; // runtime pipeline

	// impl details
	struct TypeIdWithName
	{
		size_t id;
		std::string_view name; // name is stored purely for debug purposes

		bool operator==( const TypeIdWithName& other ) const { return this->id == other.id; }
		bool operator<( const TypeIdWithName& other ) const { return this->id < other.id; }
	};

	struct RuntimeNodeInfo
	{
		size_t node_id;
		std::string_view node_name; // debug info
		BaseRenderNode* node_ptr;
		std::vector<TypeIdWithName> open_ids;
		std::vector<TypeIdWithName> write_ids;
		std::vector<TypeIdWithName> read_ids;
		std::vector<TypeIdWithName> close_ids;
	};

	std::map<size_t, RuntimeNodeInfo> CollectActiveNodes();

	struct ResourceUsers
	{
		std::vector<const RuntimeNodeInfo*> open_nodes;
		std::vector<const RuntimeNodeInfo*> write_nodes;
		std::vector<const RuntimeNodeInfo*> read_nodes;
		std::vector<const RuntimeNodeInfo*> close_nodes;
	};
	std::map<TypeIdWithName, ResourceUsers> FindResourceUsers( const std::map<size_t, RuntimeNodeInfo>& active_nodes );
	std::vector<std::pair<const RuntimeNodeInfo*, const RuntimeNodeInfo*>> BuildFrameGraphEdges( const std::map<size_t, RuntimeNodeInfo>& active_nodes );
};


#include "Pipeline.hpp"

