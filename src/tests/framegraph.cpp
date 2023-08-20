// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <boost/test/unit_test.hpp>

#include <snow_engine/stdafx.h>
#include <framegraph/Framegraph.h>

// Specify resource handlers for framegraph to use
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
// About framegraph node typedefs:
// OpenRes : node creates a resource and opens it for everyone to use. Only one active node may open a specific resource per frame
// WriteRes : node modifies a resource. It may not be the first writer and it may not be the last
// ReadRes : node reads a resource after all writers have used it. Multiple nodes may read it simultaneously
// CloseRes : node is the last user of a resource. It may do whatever it wants with the resource
//
// Each Node opens <const Node*> resource automatically when scheduled, it may be used for "Node A must be scheduled before Node B" barriers

template<class Framegraph>
class ZPrepass : public BaseRenderNode<Framegraph>
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

	ZPrepass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "Z prepass"; }
};

template<class Framegraph>
class ShadowPass : public BaseRenderNode<Framegraph>
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

	ShadowPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "shadow pass"; }
};

template<class Framegraph>
class PSSMPass : public BaseRenderNode<Framegraph>
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

	PSSMPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "PSSM pass"; }
};


template<class Framegraph>
class ForwardPass : public BaseRenderNode<Framegraph>
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
		const ZPrepass<Framegraph>* // Forward pass must happen after ZPrepass if ZPrepass exists
		>;

	using CloseRes = std::tuple
		<
		>;

	ForwardPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "Forward Pass"; }
};

template<class Framegraph>
class SkyboxPass : public BaseRenderNode<Framegraph>
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

	SkyboxPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "Skybox Pass"; }
};

template<class Framegraph>
class HBAOPass : public BaseRenderNode<Framegraph>
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

	HBAOPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "HBAO Pass"; }
};

template<class Framegraph>
class SSAOBlurPass : public BaseRenderNode<Framegraph>
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

	SSAOBlurPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "SSAO blur pass"; }
};

template<class Framegraph>
class TonemapPass : public BaseRenderNode<Framegraph>
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

	TonemapPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "tonemap pass"; }
};

template<class Framegraph>
class UIPass : public BaseRenderNode<Framegraph>
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

	UIPass( ) = default;

	virtual void Run( Framegraph& framegraph, IGraphicsCommandList& cmd_list ) override { std::cout << "ui pass"; }
};

BOOST_AUTO_TEST_SUITE( framegraph )

BOOST_AUTO_TEST_CASE( create )
{
	Framegraph
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
	> framegraph;

	// framegraph setup
	framegraph.ConstructAndEnableNode<ZPrepass>();
	framegraph.ConstructAndEnableNode<ShadowPass>();
	framegraph.ConstructAndEnableNode<PSSMPass>();
	framegraph.ConstructAndEnableNode<ForwardPass>();
	framegraph.ConstructAndEnableNode<SkyboxPass>();
	framegraph.ConstructAndEnableNode<HBAOPass>();
	framegraph.ConstructAndEnableNode<SSAOBlurPass>();
	framegraph.ConstructAndEnableNode<TonemapPass>();
	framegraph.ConstructAndEnableNode<UIPass>();

	if ( framegraph.IsRebuildNeeded() )
		framegraph.Rebuild();

	// resource binding
	framegraph.SetRes( ZBuffer() );
	framegraph.SetRes( ShadowMaps() );
	framegraph.SetRes( PSSMShadowMaps() );
	framegraph.SetRes( HDRFramebuffer() );
	framegraph.SetRes( SDRFramebuffer() );
	framegraph.SetRes( Skybox() );

	IGraphicsCommandList* cmd_lst = (IGraphicsCommandList*)1;
	// run framegraph
	framegraph.Run( *cmd_lst );

	// retrieve results
	auto& framebuffer = framegraph.GetRes<SDRFramebuffer>();

	// change framegraph
	framegraph.Disable<ZPrepass>();
	framegraph.Disable<SSAOBlurPass>();

	if ( framegraph.IsRebuildNeeded() )
		framegraph.Rebuild();

	framegraph.Run( *cmd_lst ); // should work just fine

    BOOST_TEST( true );
}

BOOST_AUTO_TEST_SUITE_END()