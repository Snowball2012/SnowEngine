#pragma once

#include "StdAfx.h"

#include "Assets.h"
#include "RHIUtils.h"

class SceneMeshInstance
{
public:
    // @todo - incapsulate as soon as it becomes more than simple getter/setter pair
    MeshAssetPtr m_asset = nullptr;

    Transform m_tf = {};

    TLAS::InstanceID m_tlas_instance = TLAS::InstanceID::nullid;
};
using SceneMeshInstanceList = packed_freelist<SceneMeshInstance>;
using SceneMeshInstanceID = SceneMeshInstanceList::id;

class Scene
{
private:
    std::unique_ptr<TLAS> m_tlas;
    SceneMeshInstanceList m_mesh_instances;

public:
    Scene();

    SceneMeshInstanceID AddMeshInstanceFromAsset( MeshAssetPtr base_asset );
    //void RemoveMeshInstance( SceneMeshInstanceID id );
    SceneMeshInstance* GetMeshInstance( SceneMeshInstanceID id ) { return m_mesh_instances.try_get( id ); };
    const SceneMeshInstance* GetMeshInstance( SceneMeshInstanceID id ) const { return m_mesh_instances.try_get( id ); }

    TLAS& GetTLAS() { return *m_tlas; }

    // Call before any render operation on the scene. This makes changes made to scene objects (mesh instances, etc.) visible to the renderer
    void Synchronize();

private:
    void UpdateTLASTransforms();
};

class SceneView
{
private:
    glm::uvec2 m_extents = glm::uvec2( 0, 0 );
    RHITexturePtr m_frame_output = nullptr;

    glm::vec3 m_eye_pos = ZeroVec3();
    glm::quat m_eye_orientation = IdentityQuat();

    float m_fovXRadians = glm::radians( 60.0f );

public:
    SceneView() {}
};