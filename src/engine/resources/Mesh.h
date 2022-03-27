#pragma once

#include "../Ptr.h"

#include <d3d12.h>

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT2 uv;
};

class StaticSubmesh
{
    // source resources
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    // derived data
    DirectX::BoundingBox m_box;
    DirectX::XMFLOAT2 m_max_inv_uv_density = {}; // for mip streaming

    // gpu res
    ComPtr<ID3D12Resource> m_vb_res;
    ComPtr<ID3D12Resource> m_ib_res;
    ComPtr<ID3D12Resource> m_blas_res;

    // gpu views
    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
    D3D12_INDEX_BUFFER_VIEW m_ibv = {};
    D3D_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
};