#pragma once

#include "StdAfx.h"

#include "WorldComponents.h"

class LevelObject;

// Immutable object for everyone except Editor. Defines some traits for level object (transform, mesh, light properties, etc.) for a piece of functionality
// Children must override OnGenerate to add ECS components to base entity or spawn new entities
// All children must be serializable
class LevelObjectTrait
{
public:
    virtual ~LevelObjectTrait() {}

    virtual bool OnGenerate( WorldEntity base_entity, LevelObject& levelobj ) const = 0;

    // Called when user can see and edit trait props
    virtual void OnUpdateGUI( bool& trait_changed ) = 0;

    // 
    virtual const char* GetTraitPrettyName() const = 0;
};

// Object definition contained in a level file. Serializable. Editor allows to create, delete and modify them.
// Each level object creates at least one world entity (and may spawn other child entities)
// Every level object associated with the world must be destroyed before the world is destroyed
class LevelObject
{
private:
    // Runtime non-serializable data
    bc::small_vector<WorldEntity, 1> m_created_entities;
    World* m_world = nullptr;

    std::string m_name;

    // @todo - I don't like having separate allocations for every trait.
    std::vector<std::unique_ptr<LevelObjectTrait>> m_traits;

public:
    explicit LevelObject( World* world );
    
    LevelObject( LevelObject&& ) = default;
    LevelObject( const LevelObject& ) = delete;

    ~LevelObject();

    // Entities must be regenerated from scratch after this. You can defer regeneration, but then you must do it manually or the object won't be updated
    bool SetName( const char* name, bool defer_regeneration );
    const char* GetName() const;

    // Entities must be regenerated from scratch after this. You can defer regeneration, but then you must do it manually or the object won't be updated
    bool AddTrait( std::unique_ptr<LevelObjectTrait>&& trait, bool defer_regeneration );

    // Returns true if entity needs to be regenerated
    bool OnUpdateGUI();

    bool RegenerateEntities();

    WorldEntity GetBaseEntity() const;

    // this interface exists for LevelObjectTraits to interact with the world (to spawn entities, add components, etc.)
    World::Entity CreateChildEntity();

    template <typename ComponentClass, typename ... Args>
    ComponentClass& AddComponent( WorldEntity entity, Args&&... args ) const;

    template <typename ComponentClass>
    ComponentClass* GetComponent( WorldEntity entity ) const;


private:

    void DestroyEntities();
    bool GenerateEntities();
};

template <typename ComponentClass, typename ... Args>
ComponentClass& LevelObject::AddComponent( WorldEntity entity, Args&&... args ) const
{
    if ( m_created_entities.empty() || entity != m_created_entities.front() )
    {
        NOTIMPL;
    }
    return m_world->AddComponent<ComponentClass>( entity, args ... );
}

template <typename ComponentClass>
ComponentClass* LevelObject::GetComponent( WorldEntity entity ) const
{
    if ( m_created_entities.empty() || entity != m_created_entities.front() )
    {
        NOTIMPL;
    }
    return m_world->GetComponent<ComponentClass>( entity );
}


class TransformTrait : public LevelObjectTrait
{
private:
    Transform m_tf = {};

    // gui state. Probably better to store it somewhere else
    bool m_drag_translate = false;
    bool m_drag_scale = false;

public:
    virtual bool OnGenerate( WorldEntity base_entity, LevelObject& levelobj ) const override;

    virtual void OnUpdateGUI( bool& trait_changed ) override;

    virtual const char* GetTraitPrettyName() const override { return "Transform"; }
};


class MeshInstanceTrait : public LevelObjectTrait
{
private:
    MeshAssetPtr m_mesh = nullptr;

    mutable std::string m_gui_path;

public:
    virtual bool OnGenerate( WorldEntity base_entity, LevelObject& levelobj ) const override;

    virtual void OnUpdateGUI( bool& trait_changed ) override;

    virtual const char* GetTraitPrettyName() const override { return "Mesh Instance"; }

    void SetAsset( MeshAssetPtr mesh );
};