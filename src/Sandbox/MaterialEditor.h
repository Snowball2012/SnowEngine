#pragma once

#include "StdAfx.h"

#include <Engine/Assets.h>
#include <Engine/LevelObjects.h>

class LevelEditor;

class MaterialEditor : public ILevelObjectEditorActions
{
    enum class PendingAction
    {
        None,
        New,
        Open,
        Close,
        OpenMaterial
    };

    MaterialAssetPtr m_material;
    MaterialAssetPtr m_pending_material;
    MaterialDescription m_edit;
    MaterialDescription m_saved;
    std::vector<MaterialAssetPtr> m_pending;
    PendingAction m_pending_action = PendingAction::None;
    bool m_visible = false;
    bool m_dirty = false;
    bool m_show_unsaved_popup = false;
    std::string m_error;
    std::array<std::string, 4> m_texture_paths;

public:
    void Show();
    virtual void OpenMaterial( MaterialAssetPtr material ) override;
    void OnUpdate( LevelEditor& editor );

private:
    bool ApplyEdit();
    void QueueUpdate( const MaterialAssetPtr& material );
    bool Save();
    bool LoadFile( const std::string& path );
    bool CreateFile( const std::string& path );
    bool CommitTexture( size_t texture_index );
    void SetMaterial( MaterialAssetPtr material );
    void RequestAction( PendingAction action, MaterialAssetPtr material = nullptr );
    void ExecutePendingAction();
    void DrawUnsavedPopup();
};
