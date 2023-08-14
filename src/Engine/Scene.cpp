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
        glm::mat4x4 model_mat;
        model_mat = glm::scale( glm::identity<glm::mat4>(), mesh_instance.m_scale );
        model_mat = model_mat * glm::toMat4( mesh_instance.m_orientation );
        model_mat = glm::translate( model_mat, mesh_instance.m_translation );

        glm::mat4x4 row_major_model = glm::transpose( model_mat );

        m_tlas->Instances()[mesh_instance.m_tlas_instance].transform = glm::mat3x4( row_major_model[0], row_major_model[1], row_major_model[2] );
    }
}