#pragma once

#include "Ptr.h"

#include <d3d12.h>

class ResizableTexture
{
public:
    ResizableTexture( ComPtr<ID3D12Resource>&& texture, ID3D12Device* device,
                      D3D12_RESOURCE_STATES initial_state, const D3D12_CLEAR_VALUE* opt_clear_value ) noexcept;

    void Resize( uint32_t width, uint32_t height );

    ID3D12Resource* Resource() const noexcept { return m_res.Get(); }

private:
    ComPtr<ID3D12Resource> m_res;
    ID3D12Device* m_device;
    std::optional<D3D12_CLEAR_VALUE> m_clear_value;
    D3D12_RESOURCE_STATES m_initial_state;
};
