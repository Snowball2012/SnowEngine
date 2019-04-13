// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "ResizableTexture.h"


ResizableTexture::ResizableTexture( ComPtr<ID3D12Resource>&& texture, ID3D12Device* device,
                                    D3D12_RESOURCE_STATES initial_state, const D3D12_CLEAR_VALUE* opt_clear_value ) noexcept
    : m_res( std::move( texture ) ), m_device( device ), m_initial_state( initial_state )
{
    assert( m_device );
    assert( m_res.Get() && m_res->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D );

    if ( opt_clear_value )
        m_clear_value = *opt_clear_value;
}


void ResizableTexture::Resize( uint32_t width, uint32_t height )
{
    assert( width > 0 && height > 0 );

    D3D12_RESOURCE_DESC desc = m_res->GetDesc();
    desc.Width = width;
    desc.Height = height;

    m_res = nullptr;
    
    ThrowIfFailed( m_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ),
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &desc, m_initial_state,
                                                      m_clear_value.has_value() ? &m_clear_value.value() : nullptr,
                                                      IID_PPV_ARGS( m_res.GetAddressOf() ) ) );
}
