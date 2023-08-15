#include "StdAfx.h"

#include "Scene.h"

Scene::Scene()
{
    m_tlas = std::make_unique<TLAS>();
}

SceneMeshInstanceID Scene::AddMeshInstanceFromAsset( MeshAssetPtr base_asset )
{
    if ( !SE_ENSURE( base_asset ) )
        return SceneMeshInstanceID::nullid;

    SceneMeshInstanceID new_mesh_id = m_mesh_instances.emplace();

    auto& mesh_instance = m_mesh_instances[new_mesh_id];

    mesh_instance.m_asset = base_asset;
    mesh_instance.m_tlas_instance = m_tlas->Instances().emplace();
    m_tlas->Instances()[mesh_instance.m_tlas_instance].blas = base_asset->GetAccelerationStructure();

    return new_mesh_id;
}

void Scene::Synchronize()
{
    UpdateTLASTransforms();
}

void Scene::UpdateTLASTransforms()
{
    for ( SceneMeshInstance& mesh_instance : m_mesh_instances )
    {
        m_tlas->Instances()[mesh_instance.m_tlas_instance].transform = ToMatrixRowMajor3x4( mesh_instance.m_tf );
    }
}