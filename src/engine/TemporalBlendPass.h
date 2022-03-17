#pragma once

#include "RenderData.h"

#include <wrl.h>
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class TemporalBlendPass
{
public:
    TemporalBlendPass( ID3D12PipelineState* pso, ID3D12RootSignature* rootsig );

    struct ShaderData
    {
        float prev_frame_blend_val;
        float unjitter[2];
        float color_window_size;
    };

    struct Context
    {
        D3D12_GPU_DESCRIPTOR_HANDLE prev_frame_srv;
        D3D12_GPU_DESCRIPTOR_HANDLE cur_frame_srv;
        D3D12_CPU_DESCRIPTOR_HANDLE cur_frame_rtv;
        ShaderData gpu_data;
    };

    void Draw( const Context& context, IGraphicsCommandList& cmd_list );

    static ComPtr<ID3D12RootSignature> BuildRootSignature( ID3D12Device& device );

    static void BuildData( DXGI_FORMAT rtv_format, ID3D12Device& device, ComPtr<ID3D12PipelineState>& pso, ComPtr<ID3D12RootSignature>& rootsig );

    static std::pair<ComPtr<ID3DBlob>, ComPtr<ID3DBlob>> LoadAndCompileShaders();

private:
    ID3D12PipelineState* m_pso = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;
};