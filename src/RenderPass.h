#pragma once

#include <d3d12.h>

// encapsulates pso, root signatures, render targets and render algorithm
class RenderPass
{
public:
	virtual ~RenderPass();
	virtual void Draw( ID3D12GraphicsCommandList& cmd_list ) = 0;
};