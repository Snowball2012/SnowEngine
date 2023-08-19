#pragma once

#include "Scene.h"

#include <ecs/EntityContainer.h>

// @todo - current ECS design requires all potential component types to be known at compile time. It is not convenient, switch to dynamic component registration later

struct NameComponent
{
	std::string name;
};

// Entities with this component will be destroyed at the end of the frame
struct DestroyEntityComponent
{
};

struct TransformComponent
{
	Transform tf;
};

struct MeshInstanceComponent
{
	SceneMeshInstanceID scene_mesh_instance = SceneMeshInstanceID::nullid;
};

struct MeshInstanceSetupComponent
{
	MeshAssetPtr asset = nullptr;
};


using World = EntityContainer<
	NameComponent,
	DestroyEntityComponent,
	TransformComponent,
	MeshInstanceComponent,
	MeshInstanceSetupComponent
>;

using WorldEntity = World::Entity;

struct WorldUtils
{
	// Scene is optional. But if any entity you want to delete has some scene objects, you have to specify the scene
	static void DestroyMarkedEntities( World& world, Scene* scene );

	// Does some common entity setup
	static void SetupEntities( World& world, Scene* scene );
};