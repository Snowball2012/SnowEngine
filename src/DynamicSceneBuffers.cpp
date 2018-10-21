#include "stdafx.h"

#include "SceneManager.h"
#include "DynamicSceneBuffers.h"

#include "RenderData.h"
#include "RenderUtils.h"
#include "Scene.h"


DynamicSceneBuffers::DynamicSceneBuffers( Microsoft::WRL::ComPtr<ID3D12Device> device, Scene* scene, size_t num_bufferized_frames ) noexcept
	: m_device( std::move( device ) ), m_buffers( num_bufferized_frames ), m_scene( scene )
{
}


void DynamicSceneBuffers::Update()
{
	if ( NeedToRebuildBuffer() )
	{
		RebuildBuffer( CurrentBuffer() );
		m_dirty_buffers_cnt--;
	}
	else
	{
		UpdateBufferContents( CurrentBuffer() );
	}

	m_cur_buffer_idx = ( m_cur_buffer_idx + 1 ) % m_buffers.size();
}


void DynamicSceneBuffers::AddTransform( TransformID id )
{
	InvalidateAllBuffers();
	m_transforms.push_back( TransformData{ id, BufferData{} } );
	m_buffer_size += Utils::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
}


void DynamicSceneBuffers::AddMaterial( MaterialID id )
{
	InvalidateAllBuffers();
	m_materials.push_back( MaterialData{ id, BufferData{} } );
	m_buffer_size += Utils::CalcConstantBufferByteSize( sizeof( MaterialConstants ) );
}


void DynamicSceneBuffers::InvalidateAllBuffers() noexcept
{
	m_dirty_buffers_cnt = m_buffers.size();
}


bool DynamicSceneBuffers::NeedToRebuildBuffer() const noexcept
{
	return m_dirty_buffers_cnt > 0;
}


DynamicSceneBuffers::BufferInstance& DynamicSceneBuffers::CurrentBuffer() noexcept
{
	return m_buffers[m_cur_buffer_idx];
}


void DynamicSceneBuffers::RebuildBuffer( BufferInstance& buffer )
{
	if ( m_buffer_size == 0 )
		return;

	if ( m_buffer_size > buffer.capacity )
	{
		if ( buffer.gpu_res )
			buffer.gpu_res->Unmap( 0, nullptr );

		buffer.gpu_res.Reset();

		constexpr float extra_buffer_space = 1.2f; // to avoid buffer recreation for every new transform/material
		buffer.capacity = m_buffer_size * 1.2;
		
		ThrowIfFailed( m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer( buffer.capacity ),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( buffer.gpu_res.GetAddressOf() ) ) );

		ThrowIfFailed( buffer.gpu_res->Map( 0, nullptr, reinterpret_cast<void**>( &buffer.mapped_data ) ) );
	}

	size_t cur_offset = 0;
	auto update_tf = [this, &buffer, &cur_offset]( TransformData& transform_data, ObjectTransform& tf )
	{
		if ( tf.IsDirty() )
			transform_data.data.dirty_buffers_cnt = m_buffers.size();
		if ( transform_data.data.dirty_buffers_cnt > 0 )
			transform_data.data.dirty_buffers_cnt--;

		transform_data.data.offset = cur_offset;

		constexpr size_t tf_gpu_size = Utils::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
		cur_offset += tf_gpu_size;

		ObjectConstants src_data = CreateTransformGPUData( tf );
		CopyToBuffer( buffer, transform_data.data, src_data );
	};

	auto update_material = [this, &buffer, &cur_offset]( MaterialData& material_data, MaterialPBR& mat )
	{
		if ( mat.IsDirty() )
			material_data.data.dirty_buffers_cnt = m_buffers.size();
		if ( material_data.data.dirty_buffers_cnt > 0 )
			material_data.data.dirty_buffers_cnt--;

		material_data.data.offset = cur_offset;

		constexpr size_t material_gpu_size = Utils::CalcConstantBufferByteSize( sizeof( MaterialConstants ) );
		cur_offset += material_gpu_size;

		MaterialConstants src_data = CreateMaterialGPUData( mat );
		CopyToBuffer( buffer, material_data.data, src_data );

	};

	UpdateAllItems( buffer, update_tf, update_material );
}


void DynamicSceneBuffers::UpdateBufferContents( BufferInstance& buffer )
{
	auto update_tf = [this, &buffer]( TransformData& transform_data, ObjectTransform& tf )
	{
		if ( tf.IsDirty() )
			transform_data.data.dirty_buffers_cnt = m_buffers.size();

		if ( transform_data.data.dirty_buffers_cnt > 0 )
		{
			ObjectConstants src_data = CreateTransformGPUData( tf );
			CopyToBuffer( buffer, transform_data.data, src_data );
			transform_data.data.dirty_buffers_cnt--;
		}
	};

	auto update_material = [this, &buffer]( MaterialData& material_data, MaterialPBR& mat )
	{
		if ( mat.IsDirty() )
			material_data.data.dirty_buffers_cnt = m_buffers.size();

		if ( material_data.data.dirty_buffers_cnt > 0 )
		{
			MaterialConstants src_data = CreateMaterialGPUData( mat );
			CopyToBuffer( buffer, material_data.data, src_data );
			material_data.data.dirty_buffers_cnt--;
		}
	};

	UpdateAllItems( buffer, update_tf, update_material );
}


ObjectConstants DynamicSceneBuffers::CreateTransformGPUData( const ObjectTransform& cpu_data ) const noexcept
{
	ObjectConstants gpu_data;
	DirectX::XMMATRIX obj2world = XMLoadFloat4x4( &cpu_data.Obj2World() );
	XMStoreFloat4x4( &gpu_data.model, XMMatrixTranspose( obj2world ) );
	XMStoreFloat4x4( &gpu_data.model_inv_transpose, XMMatrixTranspose( InverseTranspose( obj2world ) ) );
	return gpu_data;
}


MaterialConstants DynamicSceneBuffers::CreateMaterialGPUData( const MaterialPBR& cpu_data ) const noexcept
{
	MaterialConstants gpu_data;
	gpu_data.mat_transform = cpu_data.GetData().transform;
	gpu_data.diffuse_fresnel = cpu_data.GetData().diffuse_fresnel;
	return gpu_data;
}


template<typename Data>
void DynamicSceneBuffers::CopyToBuffer( BufferInstance& buffer, const BufferData& dst, const Data& src ) const noexcept
{
	memcpy( buffer.mapped_data + dst.offset, &src, sizeof( Data ) );
}


template<typename fnUseTransform, typename fnUseMaterial>
void DynamicSceneBuffers::UpdateAllItems( const BufferInstance& buffer, const fnUseTransform& fn_tf, const fnUseMaterial& fn_mat )
{
	for ( size_t mat_idx = 0; mat_idx < m_materials.size(); )
	{
		auto& material_data = m_materials[mat_idx];
		MaterialPBR* mat = m_scene->TryModifyMaterial( material_data.id );
		if ( ! mat )
		{
			if ( ( mat_idx + 1 ) != m_materials.size() )
				material_data = std::move( m_materials.back() );
			m_materials.pop_back();
			m_buffer_size -= Utils::CalcConstantBufferByteSize( sizeof( MaterialConstants ) );
		}
		else
		{
			mat_idx++;
			fn_mat( material_data, *mat );
			mat->GPUConstantBuffer() = buffer.gpu_res->GetGPUVirtualAddress() + material_data.data.offset;
		}
	}

	for ( size_t tf_idx = 0; tf_idx < m_transforms.size(); )
	{
		auto& transform_data = m_transforms[tf_idx];
		ObjectTransform* tf = m_scene->TryModifyTransform( transform_data.id );
		if ( ! tf )
		{
			if ( ( tf_idx + 1 ) != m_transforms.size() )
				transform_data = std::move( m_transforms.back() );
			m_transforms.pop_back();
			m_buffer_size -= Utils::CalcConstantBufferByteSize( sizeof( ObjectConstants ) );
		}
		else
		{
			tf_idx++;
			fn_tf( transform_data, *tf );
			tf->GPUView() = buffer.gpu_res->GetGPUVirtualAddress() + transform_data.data.offset;
		}
	}	
}
