#pragma once

// generic cpu-gpu rendering framegraph
// TODO: 2 different framegraphs ( cpu and gpu ) for cpu resources and gpu resources instead of a fused one

#include <d3d12.h>

struct ID3D12GraphicsCommandList;

template<typename Framegraph>
class BaseRenderNode
{
public:
    virtual void Run( Framegraph& framegraph, ID3D12GraphicsCommandList& cmd_list ) = 0;
};

// any resource with automatically tracked state must be derived from TrackedResource
// and mentioned in node's required resources tuple using ResourceInState template

struct TrackedResource
{
    ID3D12Resource* res;
};

template<typename Resource, D3D12_RESOURCE_STATES state,
         typename EnableIf = std::enable_if_t<
             std::is_base_of_v<TrackedResource, Resource>,
             void
         >>
struct ResourceInState;

#include "FramegraphImpl.h"

template<template <typename> class ... Nodes>
class Framegraph
{

public:
    Framegraph() = default;
    Framegraph( const Framegraph& ) = delete;
    Framegraph& operator=( const Framegraph& ) = delete;
    Framegraph( Framegraph&& ) = default;
    Framegraph& operator=( Framegraph&& ) = default;

    template<template <typename> class N, typename ... Args>
    void ConstructNode( Args&& ... args ) { m_impl.ConstructNode<N>( std::forward<Args>( args )... ); }

    template<template <typename> class N>
    void Enable() { m_impl.Enable<N>(); }

    template<template <typename> class N>
    void Disable() { m_impl.Disable<N>(); }

    template<template <typename> class N, typename ... Args>
    void ConstructAndEnableNode( Args&& ... args )
    {
        ConstructNode<N>( args... );
        Enable<N>();
    }

    template<template <typename> class N>
    N<Framegraph>* GetNode() { return m_impl.GetNode<N>(); }

    template<typename Res>
    std::optional<Res>& GetRes() { return m_impl.GetRes<Res>(); }

    template<typename Res>
    void SetRes( const Res& res ) { m_impl.SetRes<Res>( res ); }

    void Rebuild() { m_impl.Rebuild(); }
    void ClearResources() { m_impl.ClearResources(); }
    void Run( ID3D12GraphicsCommandList& cmd_list ) { m_impl.Run( cmd_list ); }

    bool IsRebuildNeeded() const { return m_impl.IsRebuildNeeded(); }

private:
    details::framegraph::FramegraphImpl<Nodes...> m_impl;
};

