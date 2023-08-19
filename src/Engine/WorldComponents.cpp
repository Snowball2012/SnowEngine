#include "StdAfx.h"

#include "WorldComponents.h"


// WorldUtils

void WorldUtils::DestroyMarkedEntities( World& world, Scene* scene )
{
    std::vector<WorldEntity> entities_to_delete;
    for ( auto&& [entity, mesh_instance, destroy_component] : world.CreateView<MeshInstanceComponent, DestroyEntityComponent>() )
    {
        if ( mesh_instance.scene_mesh_instance != SceneMeshInstanceID::nullid )
        {
            if ( !SE_ENSURE( scene ) )
            {
                return;
            }
            scene->RemoveMeshInstance( mesh_instance.scene_mesh_instance );
        }
    }

    for ( auto&& [entity, destroy_component] : world.CreateView<DestroyEntityComponent>() )
    {
        entities_to_delete.emplace_back( entity );
    }

    for ( WorldEntity entity : entities_to_delete )
    {
        world.DestroyEntity( entity );
    }
}

void WorldUtils::SetupEntities( World& world, Scene* scene )
{
    std::vector<WorldEntity> setup_entities;
    for ( auto&& [entity, mesh_instance_setup] : world.CreateView<MeshInstanceSetupComponent>() )
    {
        if ( !SE_ENSURE( scene ) )
        {
            return;
        }

        setup_entities.emplace_back( entity );

        if ( mesh_instance_setup.asset )
        {
            auto& component = world.AddComponent<MeshInstanceComponent>( entity );

            if ( !SE_ENSURE( component.scene_mesh_instance == SceneMeshInstanceID::nullid ) )
            {
                continue;
            }
            component.scene_mesh_instance = scene->AddMeshInstanceFromAsset( mesh_instance_setup.asset );
        }
    }

    for ( WorldEntity entity : setup_entities )
    {
        world.RemoveComponent<MeshInstanceSetupComponent>( entity );
    }

    setup_entities.clear();
}
