///////////////////////////////////////////////////////////////////////////////
///
///	@file  : UndoRedoSystem.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _UNDO_REDO_SYSTEM_H_
#define _UNDO_REDO_SYSTEM_H_

#include "AssetManager.h"
#include "JsonSerialize.h"
#include <stack>

namespace Framework
{
    struct AssetUndoEntry 
    {
        AssetHandle guid;               // Unique ID of the asset
        std::string typeName;           // Type name (e.g. "WindowConfig")
        std::string jsonSnapshot;       // Serialized state
    };

    struct ComponentUndoEntry 
    {
        Entity entity;
        std::string componentType;
        std::string jsonSnapshot;
    };

    class UndoRedoManager 
    {
    public:
        static UndoRedoManager& Instance()
        {
            static UndoRedoManager instance;
            return instance;
        }
        static constexpr size_t MAX_UNDO_REDO = 100;

        void PushAssetUndo(const AssetHandle& guid, const std::string& typeName, void* pObject, const prop_list& props);
        void PushComponentUndo(Entity entity, const std::string& typeName, void* pComponent, const prop_list& props);

        void UndoAsset();
        void RedoAsset();
        void UndoComponent();
        void RedoComponent();

        void Clear();

        bool CanUndoAsset() const { return !undoStack.empty(); }
        bool CanRedoAsset() const { return !redoStack.empty(); }
        bool CanUndoComponent() const { return !ecsUndoStack.empty(); }
        bool CanRedoComponent() const { return !ecsRedoStack.empty(); }

    private:
        std::vector<AssetUndoEntry> undoStack;
        std::vector<AssetUndoEntry> redoStack;
        std::vector<ComponentUndoEntry> ecsUndoStack;
        std::vector<ComponentUndoEntry> ecsRedoStack;

        void ApplyAssetSnapshot(const AssetUndoEntry& entry);
        void ApplyComponentSnapshot(const ComponentUndoEntry& entry);
    };
}
#endif // !_UNDO_REDO_SYSTEM_H_