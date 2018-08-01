#pragma once

#include <tuple>
#include <iostream>

namespace PipelineExperiments
{

	class VerySpecialResource
	{
	public:
		int a;
	};

	class BaseRenderNode
	{
	public:
		virtual void FillInput() = 0;
		virtual void Run() = 0;
	};

	template<typename PipelineInstance>
	class SpecializedNode : BaseRenderNode
	{
	public:
		SpecializedNode( int node_specific_data )
			: m_node_specific_data( node_specific_data )
		{
		}

		virtual void FillInput() override
		{
			m_pipeline->GetRes( std::get<VerySpecialResource>( m_input_data ) );
		}

		virtual void Run() override
		{
			std::cout << std::get<VerySpecialResource>( m_input_data ).a << ' ' << m_node_specific_data << std::endl;
		}

	private:
		friend PipelineInstance;
		PipelineInstance* m_pipeline;

		std::tuple<VerySpecialResource> m_input_data;

		const int m_node_specific_data;
	};

	template<template <typename> class Node>
	class Pipeline
	{
	public:
		template<typename ... NodeArgs>
		Pipeline( NodeArgs&& ...args )
			: m_node_storage( std::forward<NodeArgs>( args )... )
		{
		}

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

		void RunPipeline()
		{
			RunNode( m_node );
		}

		void RebuildPipeline()
		{
			m_node = &m_node_storage;
		}


	private:
		void RunNode( BaseRenderNode* node )
		{
			node->FillInput();
			node->Run();
		}

		decltype( Node<Pipeline>::m_input_data ) m_resources;

		Node<Pipeline> m_node_storage;
		BaseRenderNode* m_node = nullptr;
	};

	int test() {
		Pipeline<SpecializedNode> pipeline( 3 );

		pipeline.RebuildPipeline();

		pipeline.SetRes( VerySpecialResource{ 1 } );

		pipeline.RunPipeline();
	}

}