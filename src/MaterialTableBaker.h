#pragma once

#include "DescriptorTableBakery.h"

#include "SceneSystemData.h"

class MaterialTableBaker
{
public:
    MaterialTableBaker( ComPtr<ID3D12Device> device, Scene* scene, DescriptorTableBakery* tables );
    void RegisterMaterial( MaterialID id );
    void RegisterEnvMap( EnvMapID id );
    void UpdateStagingDescriptors();
    void UpdateGPUDescriptors();

private:
    using TableID = DescriptorTableBakery::TableID;
    struct MaterialData
    {
        MaterialID material_id;
        TableID table_id;
    };

    struct EnvMapData
    {
        EnvMapID env_map_id;
        TableID table_id;
    };
    
    void UpdateMaterialTextures( const MaterialPBR& material, TableID material_table_id, bool first_load );
    void UpdateEnvmapTextures( const EnviromentMap& envmap, TableID envmap_table_id, bool first_load );

    std::vector<MaterialData> m_materials;
    std::vector<EnvMapData> m_envmaps;
    DescriptorTableBakery* m_tables;

    Scene* m_scene;
    ComPtr<ID3D12Device> m_device;
};