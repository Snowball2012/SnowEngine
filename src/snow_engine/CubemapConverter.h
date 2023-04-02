#pragma once

#include "Ptr.h"

#include <d3d12.h>

class CubemapConverter
{
public:
    CubemapConverter( ComPtr<ID3D12Device> device );

    ComPtr<ID3D12Resource> MakeCubemapFromCylindrical( uint32_t resolution, D3D12_GPU_DESCRIPTOR_HANDLE texture_srv,
                                                       DXGI_FORMAT cubemap_format, IGraphicsCommandList& cmd_list );

private:
    struct Shaders
    {
        ComPtr<ID3D10Blob> vs;
        ComPtr<ID3D10Blob> ps;
    };

    std::unordered_map<DXGI_FORMAT, ComPtr<ID3D12PipelineState>> m_pso;
    ComPtr<ID3D12RootSignature> m_rs;
    Shaders m_shaders;
    ComPtr<ID3D12Resource> m_gpu_matrices;

    ComPtr<ID3D12Device> m_device;

    void Init();

    ComPtr<ID3D12RootSignature> BuildRootSignature() const;
    static Shaders CompileShaders();
    ComPtr<ID3D12PipelineState> BuildPSO( DXGI_FORMAT rtv_format, ID3D12RootSignature* rs ) const;
    ComPtr<ID3D12Resource> BuildGPUMatrices() const;

    D3D12_RESOURCE_DESC GetCubemapDesc( uint32_t resolution, DXGI_FORMAT cubemap_format ) const;


    ComPtr<ID3D12DescriptorHeap> cubemap_rtv_heap;
};