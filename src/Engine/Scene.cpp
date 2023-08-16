#include "StdAfx.h"

#include "Scene.h"


// Sceme

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


// SceneView

void SceneView::SetLookAt( const glm::vec3& eye, const glm::vec3& center )
{
    m_eye_pos = eye;
    m_eye_orientation = glm::quatLookAt( glm::normalize( center - eye ), m_up );
}

void SceneView::SetExtents( const glm::uvec2& extents )
{
    m_extents = extents;

    RHI::TextureInfo tex_info = {};
    tex_info.dimensions = RHITextureDimensions::T2D;
    tex_info.width = m_extents.x;
    tex_info.height = m_extents.y;
    tex_info.depth = 1;
    tex_info.mips = 1;
    tex_info.array_layers = 1;
    tex_info.format = RHIFormat::R8G8B8A8_UNORM;
    tex_info.usage = RHITextureUsageFlags::TextureROView | RHITextureUsageFlags::TextureRWView;
    tex_info.initial_layout = RHITextureLayout::ShaderReadOnly;

    m_rt_frame = GetRHI().CreateTexture( tex_info );

    RHI::TextureRWViewInfo uav_info = {};
    uav_info.texture = m_rt_frame.get();
    m_frame_rwview = GetRHI().CreateTextureRWView(uav_info);

    RHI::TextureROViewInfo srv_info = {};
    srv_info.texture = m_rt_frame.get();
    m_frame_roview = GetRHI().CreateTextureROView(srv_info);
}

glm::mat4x4 SceneView::CalcViewMatrix() const
{
    // @todo - i don't like inverses here
    return glm::translate( glm::toMat4( glm::inverse( m_eye_orientation ) ), -m_eye_pos );
}

glm::mat4x4 SceneView::CalcProjectionMatrix() const
{
    float aspectRatio = float( m_extents.x ) / float( m_extents.y );
    float fovY = m_fovXRadians / aspectRatio;
    return glm::perspective( fovY, aspectRatio, 0.1f, 10.0f );
}
