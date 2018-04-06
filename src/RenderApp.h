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
	void UpdateAndWaitForFrameResource();
	// isolated from FrameResources logic
	void UpdatePassConstants( const GameTimer& gt, UploadBuffer<PassConstants>& pass_cb );
	void UpdateLights( PassConstants& pc );
	void UpdateRenderItem( RenderItem& renderitem, UploadBuffer<ObjectConstants>& obj_cb );
	void UpdateDynamicGeometry( UploadBuffer<Vertex>& dynamic_vb );
	void UpdateWaves( const GameTimer& gt );

	virtual void Draw( const GameTimer& gt ) override;
	virtual void Draw_MainPass( ID3D12GraphicsCommandList* cmd_list );

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
	void BuildMaterials();
	void BuildLights();
	void BuildRenderItems();
	void BuildPSO();
	void BuildFrameResources();

	// geometry
	template <DXGI_FORMAT index_format, class VCRange, class ICRange>
	std::unordered_map<std::string, SubmeshGeometry>& LoadStaticGeometry( std::string name, const VCRange& vertices, const ICRange& indices, ID3D12GraphicsCommandList* cmd_list );
	std::unordered_map<std::string, MeshGeometry> m_static_geometry;

	template <DXGI_FORMAT index_format, class ICRange>
	void LoadDynamicGeometryIndices( const ICRange& indices, ID3D12GraphicsCommandList* cmd_list );
	MeshGeometry m_dynamic_geometry;

	// lighting and materials
	std::unordered_map<std::string, StaticMaterial> m_materials;
	std::unordered_map<std::string, Light> m_lights;

	// waves
	std::vector<Vertex> m_waves_cpu_vertices;
	std::vector<uint16_t> m_waves_cpu_indices;

	// resources
	static constexpr int num_frame_resources = 3;
	std::vector<FrameResource> m_frame_resources;
	FrameResource* m_cur_frame_resource = nullptr;
	int m_cur_fr_idx = 0;
	static constexpr int num_passes = 1;

	// scene (render items)
	std::vector<RenderItem> m_renderitems;

	// pipeline
	ComPtr<ID3D12RootSignature> m_root_signature = nullptr;
	
	ComPtr<ID3DBlob> m_vs_bytecode = nullptr;
	ComPtr<ID3DBlob> m_ps_bytecode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_input_layout;

	ComPtr<ID3D12PipelineState> m_pso_main = nullptr;
	ComPtr<ID3D12PipelineState> m_pso_wireframe = nullptr;

	// camera
	DirectX::XMFLOAT4X4 m_view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_proj = MathHelper::Identity4x4();

	float m_theta = 1.5f * DirectX::XM_PI;
	float m_phi = DirectX::XM_PIDIV2;
	float m_radius = 5.0f;

	bool m_wireframe_mode = false;

	// inputs
	POINT m_last_mouse_pos;
	LPSTR m_cmd_line;
};
