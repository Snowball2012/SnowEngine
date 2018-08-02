#include "stdafx.h"

#include "Pipeline.h"

#include "PipelineResource.h"
namespace Testing
{
	template<typename Pipeline>
	struct NodeA
	{
		int a;
	};

	template<typename Pipeline>
	struct NodeB
	{
		int b;
	};

	template<typename Pipeline>
	struct NodeC
	{
		NodeC( int a, int b, int c )
		{}

		int c;
	};

	void create_pipeline()
	{
		Pipeline<NodeA, NodeB, NodeC> pipeline;

		pipeline.ConstructNode<NodeA>();
		pipeline.ConstructNode<NodeC>( 1, 2, 3 );
		pipeline.Enable<NodeA>();
		pipeline.Disable<NodeA>();
		pipeline.Enable<NodeB>();
		if ( pipeline.IsRebuildNeeded() )
			pipeline.RebuildPipeline();

		auto node_c = pipeline.GetNode<NodeC>();
		if ( node_c )
			node_c->c = 3;
	}

}