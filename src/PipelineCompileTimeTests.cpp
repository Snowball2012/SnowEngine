#include "stdafx.h"

#include "Pipeline.h"

#include "PipelineResource.h"
namespace Testing
{
	// resource handles here must be lightweight. Try not to store the data itself here, only copyable handles/pointers with default constructors
	struct ShadowCasters
	{};

	struct ShadowProducers
	{};

	struct ShadowMaps
	{};

	struct ShadowMapStorage
	{};

	struct HDRColorStorage
	{};

	struct HDRColorOut
	{};

	struct TonemapSettings
	{};

	struct BackbufferStorage
	{};

	struct TonemappedBackbuffer
	{};

	struct DepthStorage
	{};

	struct FinalSceneDepth
	{};

	struct ScreenConstants
	{};

	struct SceneContext
	{};

	struct PreviousFrameStorage
	{};

	struct PreviousFrame
	{};

	struct NewPreviousFrame
	{};

	struct FinalBackbuffer
	{};;

	template<class Pipeline>
	class ForwardPassNode : public BaseRenderNode
	{
	public:
		using InputResources = std::tuple
		<
			ShadowMaps,
			HDRColorStorage,
			DepthStorage,
			ScreenConstants,
			SceneContext
		>;

		using OutputResources = std::tuple
		<
			HDRColorOut,
			FinalSceneDepth
		>;

		virtual void Run() override { std::cout << "Forward Pass"; }
	};

	template<class Pipeline>
	class ShadowPassNode : public BaseRenderNode
	{
	public:
		using InputResources = std::tuple
			<
			ShadowCasters,
			ShadowProducers,
			ShadowMapStorage
			>;

		using OutputResources = std::tuple
			<
			ShadowMaps
			>;

		virtual void Run() override { std::cout << "Shadow Pass"; }
	};

	template<class Pipeline>
	class ToneMapPassNode : public BaseRenderNode
	{
	public:
		using InputResources = std::tuple
			<
			HDRColorOut,
			FinalSceneDepth,
			PreviousFrame,
			PreviousFrameStorage,
			TonemapSettings,
			BackbufferStorage
			>;

		using OutputResources = std::tuple
			<
			NewPreviousFrame,
			TonemappedBackbuffer
			>;

		virtual void Run() override { std::cout << "Tonemap Pass"; }
	};

	template<class Pipeline>
	class UIPassNode : public BaseRenderNode
	{
	public:
		using InputResources = std::tuple
			<
			TonemappedBackbuffer
			>;

		using OutputResources = std::tuple
			<
			FinalBackbuffer
			>;

		virtual void Run() override { std::cout << "UI Pass"; }
	};


	void create_pipeline()
	{
		Pipeline<ForwardPassNode, ShadowPassNode, ToneMapPassNode, UIPassNode> pipeline;

		// pipeline setup
		pipeline.ConstructNode<ForwardPassNode>();
		pipeline.ConstructNode<ShadowPassNode>();
		pipeline.ConstructNode<ToneMapPassNode>();
		pipeline.ConstructNode<UIPassNode>();
		pipeline.Enable<ForwardPassNode>();
		pipeline.Enable<ShadowPassNode>();
		pipeline.Enable<ToneMapPassNode>();
		pipeline.Enable<UIPassNode>();

		if ( pipeline.IsRebuildNeeded() )
			pipeline.RebuildPipeline();

		// resource binding
		pipeline.SetRes( HDRColorStorage() );
		pipeline.SetRes( ScreenConstants() );
		pipeline.SetRes( DepthStorage() );
		pipeline.SetRes( SceneContext() );
		pipeline.SetRes( ShadowCasters() );
		pipeline.SetRes( ShadowMapStorage() );
		pipeline.SetRes( ShadowProducers() );
		pipeline.SetRes( PreviousFrameStorage() );
		pipeline.SetRes( TonemapSettings() );
		pipeline.SetRes( BackbufferStorage() );

		// run pipeline
		pipeline.Run();

		// retrieve results
		NewPreviousFrame npf;
		pipeline.GetRes( npf );
		FinalBackbuffer res_backbuffer;
		pipeline.GetRes( res_backbuffer );
	}

}