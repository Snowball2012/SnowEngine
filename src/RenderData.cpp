#include "stdafx.h"
#include "RenderData.h"

void StaticMaterial::LoadToGPU( ID3D12Device& device, ID3D12GraphicsCommandList& cmd_list )
{
	cb_gpu = d3dUtil::CreateDefaultBuffer( &device, &cmd_list, &mat_constants, sizeof( mat_constants ), cb_uploader );
}
