#pragma once

#include <Luna/d3dApp.h>
#include <Luna/MathHelper.h>
#include <Luna/UploadBuffer.h>

#include "FrameResource.h"
#include "ObjImporter.h"


class RenderApp: public D3DApp
{
public:
	RenderApp( HINSTANCE hinstance, LPSTR cmd_line );
	RenderApp( const RenderApp& rhs ) = delete;
	RenderApp& operator=( const RenderApp& rhs ) = delete;

	// Inherited via D3DApp
	virtual bool Initialize() override;

private:

	// D3DApp
	virtual void OnResize() override;

	virtual void Update( const GameTimer& gt ) override;
	virtual void Draw( const GameTimer& gt ) override;

	virtual void OnMouseDown( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseUp( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseMove( WPARAM btnState, int x, int y ) override;

	virtual LRESULT MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) override;

	// MsgProc event handlers
	void OnKeyUp( WPARAM btn );

	// build functions
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSO();
	void BuildFrameResources();

	template <DXGI_FORMAT index_format, class VCRange, class ICRange>
	std::unordered_map<std::string, SubmeshGeometry>& LoadGeometry( std::string name, const VCRange& vertices, const ICRange& indices );


	static const int num_frame_resources = 3;
	std::vector<FrameResource> m_frame_resources;
	FrameResource* m_cur_frame_resource = nullptr;
	int m_cur_fr_idx = 0;

	ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_cbv_heap = nullptr;

	boost::optional<UploadBuffer<ObjectConstants>> m_object_cb;

	std::unordered_map<std::string, MeshGeometry> m_geometry;

	ComPtr<ID3DBlob> m_vs_bytecode = nullptr;
	ComPtr<ID3DBlob> m_ps_bytecode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_input_layout;

	ComPtr<ID3D12PipelineState> m_pso = nullptr;

	DirectX::XMFLOAT4X4 m_world = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_proj = MathHelper::Identity4x4();

	float m_theta = 1.5f * DirectX::XM_PI;
	float m_phi = DirectX::XM_PIDIV4;
	float m_radius = 5.0f;

	POINT m_last_mouse_pos;

	LPSTR m_cmd_line;
};
