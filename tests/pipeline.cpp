// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define BOOST_TEST_MODULE main
#include <boost/test/included/unit_test.hpp>

//#include <boost/test/unit_test.hpp>

#include "../src/stdafx.h"
#include "../src/Pipeline.h"

#include <iostream>

// Specify resource handlers for pipeline to use
// resource handles here must be lightweight. Try not to store the data itself here, only copyable handles/pointers with default constructors
struct ZBuffer
{};

struct ShadowMaps
{};

struct PSSMShadowMaps
{};

struct HDRFramebuffer
{};

struct SDRFramebuffer
{};

struct Ambient
{};

struct Normals
{};

struct Skybox
{};

struct SSAOBuffer
{};

struct SSAOBufferBlurred
{};

// Render nodes
//
// About pipeline node typedefs:
// OpenRes : node creates a resource and opens it for everyone to use. Only one active node may open a specific resource per frame
// WriteRes : node modifies a resource. It may not be the first writer and it may not be the last
// ReadRes : node reads a resource after all writers have used it. Multiple nodes may read it simultaneously
// CloseRes : node is the last user of a resource. It may do whatever it wants with the resource
//
// Each Node opens <const Node*> resource automatically when scheduled, it may be used for "Node A must be scheduled before Node B" barriers

template<class Pipeline>
class ZPrepass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		ZBuffer
		>;

	using ReadRes = std::tuple
		<
		>;		

	using CloseRes = std::tuple
		<
		>;

	ZPrepass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "Z prepass"; }
};

template<class Pipeline>
class ShadowPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		ShadowMaps
		>;

	using ReadRes = std::tuple
		<
		>;

	using CloseRes = std::tuple
		<
		>;

	ShadowPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "shadow pass"; }
};

template<class Pipeline>
class PSSMPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		PSSMShadowMaps
		>;

	using ReadRes = std::tuple
		<
		>;

	using CloseRes = std::tuple
		<
		>;

	PSSMPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "PSSM pass"; }
};


template<class Pipeline>
class ForwardPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		HDRFramebuffer,
		Ambient,
		Normals,
		ZBuffer
		>;

	using ReadRes = std::tuple
		<
		PSSMShadowMaps,
		ShadowMaps,
		const ZPrepass<Pipeline>* // Forward pass must happen after ZPrepass if ZPrepass exists
		>;

	using CloseRes = std::tuple
		<
		>;

	ForwardPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "Forward Pass"; }
};

template<class Pipeline>
class SkyboxPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		HDRFramebuffer
		>;

	using ReadRes = std::tuple
		<
		Skybox,
		ZBuffer
		>;

	using CloseRes = std::tuple
		<
		>;

	SkyboxPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "Skybox Pass"; }
};

template<class Pipeline>
class HBAOPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		SSAOBuffer
		>;

	using ReadRes = std::tuple
		<
		ZBuffer,
		Normals
		>;

	using CloseRes = std::tuple
		<
		>;

	HBAOPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "HBAO Pass"; }
};

template<class Pipeline>
class SSAOBlurPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		SSAOBufferBlurred
		>;

	using ReadRes = std::tuple
		<
		SSAOBuffer,
		ZBuffer
		>;

	using CloseRes = std::tuple
		<
		>;

	SSAOBlurPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "SSAO blur pass"; }
};

template<class Pipeline>
class TonemapPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		SDRFramebuffer
		>;

	using ReadRes = std::tuple
		<
		HDRFramebuffer,
		Ambient,
		SSAOBuffer,        // a node can decide which resource to use in runtime by checking which resource exists in runtime
		SSAOBufferBlurred  //
		>;

	using CloseRes = std::tuple
		<
		>;

	TonemapPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "tonemap pass"; }
};

template<class Pipeline>
class UIPass : public BaseRenderNode
{
public:
	using OpenRes = std::tuple
		<
		>;

	using WriteRes = std::tuple
		<
		>;

	using ReadRes = std::tuple
		<
		>;

	using CloseRes = std::tuple
		<
		SDRFramebuffer
		>;

	UIPass( Pipeline* pipeline )
	{}

	virtual void Run( ID3D12GraphicsCommandList& cmd_list ) override { std::cout << "ui pass"; }
};

BOOST_AUTO_TEST_SUITE( pipeline )

BOOST_AUTO_TEST_CASE( create )
{
	Pipeline
	<
		ZPrepass,
		ShadowPass,
		PSSMPass,
		ForwardPass,
		SkyboxPass,
		HBAOPass,
		SSAOBlurPass,
		TonemapPass,
		UIPass
	> pipeline;

	// pipeline setup
	pipeline.ConstructAndEnableNode<ZPrepass>();
	pipeline.ConstructAndEnableNode<ShadowPass>();
	pipeline.ConstructAndEnableNode<PSSMPass>();
	pipeline.ConstructAndEnableNode<ForwardPass>();
	pipeline.ConstructAndEnableNode<SkyboxPass>();
	pipeline.ConstructAndEnableNode<HBAOPass>();
	pipeline.ConstructAndEnableNode<SSAOBlurPass>();
	pipeline.ConstructAndEnableNode<TonemapPass>();
	pipeline.ConstructAndEnableNode<UIPass>();

	if ( pipeline.IsRebuildNeeded() )
		pipeline.RebuildPipeline();

	// resource binding
	pipeline.SetRes( ZBuffer() );
	pipeline.SetRes( ShadowMaps() );
	pipeline.SetRes( PSSMShadowMaps() );
	pipeline.SetRes( HDRFramebuffer() );
	pipeline.SetRes( SDRFramebuffer() );
	pipeline.SetRes( Skybox() );

	ID3D12GraphicsCommandList* cmd_lst = nullptr;
	// run pipeline
	pipeline.Run( *cmd_lst );

	// retrieve results
	auto& framebuffer = pipeline.GetRes<SDRFramebuffer>();

	// change pipeline
	pipeline.Disable<ZPrepass>();
	pipeline.Disable<SSAOBlurPass>();

	if ( pipeline.IsRebuildNeeded() )
		pipeline.RebuildPipeline();

	pipeline.Run( *cmd_lst ); // should work just fine
}

BOOST_AUTO_TEST_SUITE_END()