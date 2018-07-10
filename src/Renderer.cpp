#include "stdafx.h"

#include "Renderer.h"

#include <imgui/imgui.h>
#include "imgui_impl/imgui_impl_win32.h"
#include "imgui_impl/imgui_impl_dx12.h"

#include "GeomGeneration.h"

#include "ForwardLightingPass.h"
#include "DepthOnlyPass.h"
#include "TemporalBlendPass.h"

#include <dxtk12/DDSTextureLoader.h>
#include <dxtk12/DirectXHelpers.h>

#include <d3d12sdklayers.h>

#include <future>

using namespace DirectX;

void Renderer::Init( const ImportedScene& ext_scene )
{
	ThrowIfFailed( m_cmd_list->Reset( m_direct_cmd_allocator.Get(), nullptr ) );

	BuildDescriptorHeaps( ext_scene );
	RecreatePrevFrameTexture();
	InitGUI();

	LoadAndBuildTextures( ext_scene, true );
	BuildGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildLights();
	BuildConstantBuffers();
	BuildPasses();

	ThrowIfFailed( m_cmd_list->Close() );
	ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
	m_cmd_queue->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );
	FlushCommandQueue();

	DisposeUploaders();
	DisposeCPUGeom();
	m_imported_scene.indices.clear();
	m_imported_scene.materials.clear();
	m_imported_scene.submeshes.clear();
	m_imported_scene.textures.clear();
	m_imported_scene.vertices.clear();

	m_imported_scene.indices.shrink_to_fit();
	m_imported_scene.materials.shrink_to_fit();
	m_imported_scene.submeshes.shrink_to_fit();
	m_imported_scene.textures.shrink_to_fit();
	m_imported_scene.vertices.shrink_to_fit();
}

void Renderer::BuildDescriptorHeaps( const ImportedScene& ext_scene )
{
	// srv heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
		srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srv_heap_desc.NumDescriptors = 3/*default textures*/ + UINT( ext_scene.textures.size() )
			+ 1 * num_frame_resources /*shadow maps*/
			+ 1 /*imgui font*/
			+ 1 /*previous frame*/;

		ComPtr<ID3D12DescriptorHeap> srv_heap;
		ThrowIfFailed( m_d3d_device->CreateDescriptorHeap( &srv_heap_desc, IID_PPV_ARGS( &srv_heap ) ) );
		m_srv_heap = std::make_unique<DescriptorHeap>( std::move( srv_heap ), m_cbv_srv_uav_size );
	}
	// secondary dsv heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc{};
		dsv_heap_desc.NumDescriptors = 1 * num_frame_resources; // shadow maps
		dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

		ComPtr<ID3D12DescriptorHeap> dsv_heap;
		ThrowIfFailed( m_d3d_device->CreateDescriptorHeap( &dsv_heap_desc, IID_PPV_ARGS( &dsv_heap ) ) );
		m_dsv_heap = std::make_unique<DescriptorHeap>( std::move( dsv_heap ), m_dsv_size );
	}
}

void Renderer::RecreatePrevFrameTexture()
{
	assert( m_srv_heap );

	auto recreate_tex = [&]( auto& tex )
	{
		tex.texture_gpu = nullptr;

		CD3DX12_RESOURCE_DESC tex_desc( CurrentBackBuffer()->GetDesc() );
		D3D12_CLEAR_VALUE opt_clear;
		opt_clear.Color[0] = 0;
		opt_clear.Color[1] = 0;
		opt_clear.Color[2] = 0;
		opt_clear.Color[3] = 0;
		opt_clear.Format = mBackBufferFormat;
		opt_clear.DepthStencil.Depth = 1.0f;
		opt_clear.DepthStencil.Stencil = 0;
		ThrowIfFailed( m_d3d_device->CreateCommittedResource( &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ), D3D12_HEAP_FLAG_NONE,
																&tex_desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																&opt_clear, IID_PPV_ARGS( &tex.texture_gpu ) ) );

		tex.srv.reset();
		tex.srv = std::make_unique<Descriptor>( std::move( m_srv_heap->AllocateDescriptor() ) );
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		{
			srv_desc.Format = tex_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = 1;
		}
		m_d3d_device->CreateShaderResourceView( tex.texture_gpu.Get(), &srv_desc, tex.srv->HandleCPU() );
	};

	recreate_tex( m_prev_frame_texture );
	recreate_tex( m_jittered_frame_texture );
}

void Renderer::InitGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& imgui_io = ImGui::GetIO();
	ImGui_ImplWin32_Init( m_main_hwnd );
	m_ui_font_desc = std::make_unique<Descriptor>( std::move( m_srv_heap->AllocateDescriptor() ) );
	ImGui_ImplDX12_Init( m_d3d_device.Get(), num_frame_resources, m_back_buffer_format, m_ui_font_desc->HandleCPU(), m_ui_font_desc->HandleGPU() );
	ImGui::StyleColorsDark();
}

void Renderer::LoadAndBuildTextures( const ImportedScene& ext_scene, bool flush_per_texture )
{
	LoadStaticDDSTexture( L"resources/textures/WoodCrate01.dds", "placeholder_albedo" );
	LoadStaticDDSTexture( L"resources/textures/default_deriv_normal.dds", "placeholder_normal" );
	LoadStaticDDSTexture( L"resources/textures/default_spec.dds", "placeholder_specular" );
	for ( size_t i = 0; i < ext_scene.textures.size(); ++i )
	{
		LoadStaticDDSTexture( std::wstring( ext_scene.textures[i].begin(), ext_scene.textures[i].end() ).c_str(), ext_scene.textures[i] );

		if ( flush_per_texture )
		{
			ThrowIfFailed( m_cmd_list->Close() );
			ID3D12CommandList* cmd_lists[] = { m_cmd_list.Get() };
			m_cmd_queue->ExecuteCommandLists( _countof( cmd_lists ), cmd_lists );
			FlushCommandQueue();
			ThrowIfFailed( m_cmd_list->Reset( m_direct_cmd_allocator.Get(), nullptr ) );

			m_scene.textures[ext_scene.textures[i]].texture_uploader = nullptr;
		}
	}
}

void Renderer::LoadStaticDDSTexture( const wchar_t * filename, const std::string & name )
{
	auto& texture = m_scene.textures.emplace( name, StaticTexture() ).first->second;

	std::unique_ptr<uint8_t[]> dds_data;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ThrowIfFailed( DirectX::LoadDDSTextureFromFile( m_d3d_device.Get(), filename, texture.texture_gpu.GetAddressOf(), dds_data, subresources ) );

	size_t total_size = GetRequiredIntermediateSize( texture.texture_gpu.Get(), 0, UINT( subresources.size() ) );

	// create upload buffer
	ThrowIfFailed( m_d3d_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer( total_size ),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS( texture.texture_uploader.GetAddressOf() ) ) ); // TODO: not always committed?

																	 // base resource is now in COPY_DEST STATE, no barrier required 
	UpdateSubresources( m_cmd_list.Get(), texture.texture_gpu.Get(), texture.texture_uploader.Get(), 0, 0, UINT( subresources.size() ), subresources.data() );
	m_cmd_list->ResourceBarrier( 1, &CD3DX12_RESOURCE_BARRIER::Transition( texture.texture_gpu.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ ) );

	texture.srv = std::make_unique<Descriptor>( m_srv_heap->AllocateDescriptor() );
	CreateShaderResourceView( m_d3d_device.Get(), texture.texture_gpu.Get(), texture.srv->HandleCPU() );
}

void Renderer::FlushCommandQueue()
{
	NOTIMPL;
}

ID3D12Resource* Renderer::CurrentBackBuffer()
{
	NOTIMPL;
	return nullptr;
}
