#include "stdafx.h"

#include "SceneManager.h"
#include "MaterialTableBaker.h"


MaterialTableBaker::MaterialTableBaker( ComPtr<ID3D12Device> device, Scene* scene, DescriptorTableBakery* tables )
	: m_scene( scene ), m_tables( tables ), m_device( std::move( device ) )
{
}


void MaterialTableBaker::RegisterMaterial( MaterialID id )
{
	m_materials.emplace_back();
	auto& material_data = m_materials.back();
	material_data.table_id = m_tables->AllocateTable( 3 );
	material_data.material_id = id;
	assert( m_scene->AllMaterials().has( id ) );
	UpdateMaterialTextures( m_scene->AllMaterials()[id], material_data.table_id, true );
}


void MaterialTableBaker::UpdateStagingDescriptors()
{
	for ( size_t i = 0; i < m_materials.size(); )
	{
		auto& material_data = m_materials[i];
		MaterialPBR* mat = m_scene->TryModifyMaterial( material_data.material_id );
		if ( ! mat )
		{
			m_tables->EraseTable( material_data.table_id );
			if ( ( i + 1 ) != m_materials.size() )
				material_data = std::move( m_materials.back() );
			m_materials.pop_back();
		}
		else
		{
			i++;
			UpdateMaterialTextures( *mat, material_data.table_id, false );
		}
	}
}


// DescriptorTableBakery::BakeGPUTables() must be called before this method
void MaterialTableBaker::UpdateGPUDescriptors()
{
	for ( const auto& material_data : m_materials )
	{
		assert( m_scene->AllMaterials().has( material_data.material_id ) );
		auto table = m_tables->GetTable( material_data.table_id );
		assert( table.has_value() );

		m_scene->TryModifyMaterial( material_data.material_id )->DescriptorTable() = table->gpu_handle;
	}
}


void MaterialTableBaker::UpdateMaterialTextures( const MaterialPBR& material, TableID material_table_id, bool is_first_load )
{
	auto update_texture = [&]( TextureID tex_id, INT offset ) -> void
	{
		assert( m_scene->AllTextures().has( tex_id ) );

		const Texture& tex = m_scene->AllTextures()[tex_id];
		if ( tex.IsLoaded() && ( tex.IsDirty() || is_first_load ) )
		{
			std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> dest_table = m_tables->ModifyTable( material_table_id );
			assert( dest_table.has_value() );
			m_device->CopyDescriptorsSimple( 1,
											 CD3DX12_CPU_DESCRIPTOR_HANDLE( *dest_table, offset, UINT( m_tables->GetDescriptorIncrementSize() ) ),
											 tex.StagingSRV(),
											 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
		}
	};

	update_texture( material.Textures().base_color, 0 );
	update_texture( material.Textures().normal, 1 );
	update_texture( material.Textures().specular, 2 );
}
