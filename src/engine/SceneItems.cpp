// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"
#include "SceneItems.h"

std::pair<uint64_t, uint64_t> MaterialPBR::GetPipelineStateID( FramegraphTechnique technique ) const
{
    NOTIMPL;
    return std::pair<uint64_t, uint64_t>();
}

bool MaterialPBR::BindDataToPipeline( FramegraphTechnique technique, uint64_t item_id, ID3D12GraphicsCommandList& cmd_list ) const
{
    switch ( technique )
    {
        case FramegraphTechnique::DepthPass:
        case FramegraphTechnique::ShadowGenPass:
            cmd_list.SetGraphicsRootDescriptorTable( 1, DescriptorTable() );
            break;
        case FramegraphTechnique::ForwardZPass:
            cmd_list.SetGraphicsRootDescriptorTable( 2, DescriptorTable() );
            cmd_list.SetGraphicsRootConstantBufferView( 1, GPUConstantBuffer() );
            break;
        default:
            return false;
    }

    return true;
}
