#pragma once

#include "utils/UniqueTuple.h"

class BaseRenderNode
{
public:
	virtual void Run() = 0;
};

template<template <typename> class ... Node>
class Pipeline
{
public:
	template<template <typename> class N, typename ... Args>
	void ConstructNode( Args&& ... args )
	{
		auto& node_storage = std::get<NStorage<N<Pipeline>>>( m_node_storage );
		node_storage.node.emplace( std::forward<Args>( args )... );
	}

	template<template <typename> class N>
	void Enable()
	{
		auto& node_storage = std::get<NStorage<N<Pipeline>>>( m_node_storage );

		if ( ! node_storage.node.is_initialized() )
			throw SnowEngineException( "node is not constructed yet" );
		node_storage.enabled = true;

		m_need_to_rebuild_pipeline = true;
	}

	template<template <typename> class N>
	void Disable()
	{
		auto& node_storage = std::get<NStorage<N<Pipeline>>>( m_node_storage );

		if ( ! node_storage.node.is_initialized() )
			throw SnowEngineException( "node is not constructed yet" );

		node_storage.enabled = false;

		m_need_to_rebuild_pipeline = true;
	}

	template<template <typename> class N>
	N<Pipeline>* GetNode();

	template<typename Res>
	void GetRes( Res& res )
	{
		res = std::get<Res>( m_resources );
	}

	template<typename Res>
	void SetRes( const Res& res )
	{
		std::get<Res>( m_resources ) = res;
	}


	void RebuildPipeline();
	void Run();

	bool IsRebuildNeeded() const
	{
		return m_need_to_rebuild_pipeline;
	}

private:

	template<typename N>
	struct NStorage
	{
		boost::optional<N> node = boost::none;
		bool enabled = false;
	};

	std::tuple<NStorage<Node<Pipeline>>...> m_node_storage;

	bool m_need_to_rebuild_pipeline = true;

	template<typename Node>
	using NodeResources = UniqueTuple<decltype( std::tuple_cat( std::declval<typename Node::InputResources>(),
																std::declval<typename Node::OutputResources>() ) )>;

	using PipelineResources = UniqueTuple<decltype( std::tuple_cat( std::declval<NodeResources<Node<Pipeline>>>()... ) )>;

	PipelineResources m_resources;
};


#include "Pipeline.hpp"

