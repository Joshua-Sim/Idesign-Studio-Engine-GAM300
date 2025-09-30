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

// 27 Sept 2025 - Edwin
// Untested, would nid to review in the future 

// Define the UndoEntry
// Create Undo/Redo Stacks
// Push to Undo before makiung changes
// Undo
// Redo

namespace Framework
{
    struct AssetUndoEntry 
    {
        AssetHandle guid;               // Unique ID of the asset
        std::string typeName;           // Type name (e.g. "WindowConfig")
        std::string jsonSnapshot;       // Serialized state
    };

    class AssetUndoRedoManager 
    {
    public:
        static constexpr size_t MAX_UNDO_REDO = 100;

        void PushUndo(const AssetHandle& guid, const std::string& typeName, void* pObject, const prop_list& props) 
        {
            // Serialize current state
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();
            Serialize(pObject, props, doc, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            // Push to undo stack
            undoStack.push_back({ guid, typeName, buffer.GetString() });
            redoStack.clear();

            if (undoStack.size() > MAX_UNDO_REDO)
                undoStack.erase(undoStack.begin());
        }

        void Undo() 
        {
            if (undoStack.empty()) return;

            AssetUndoEntry entry = undoStack.back();
            undoStack.pop_back();
            redoStack.push_back(entry);

            ApplySnapshot(entry);
        }

        void Redo() 
        {
            if (redoStack.empty()) return;

            AssetUndoEntry entry = redoStack.back();
            redoStack.pop_back();
            undoStack.push_back(entry);

            ApplySnapshot(entry);
        }

        void Clear() 
        {
            undoStack.clear();
            redoStack.clear();
        }

        bool CanUndo() const { return !undoStack.empty(); }
        bool CanRedo() const { return !redoStack.empty(); }

    private:
        std::vector<AssetUndoEntry> undoStack;
        std::vector<AssetUndoEntry> redoStack;

        void ApplySnapshot(const AssetUndoEntry& entry) 
        {
            auto& manager = AssetManager::Instance();

            // Get the asset entry
            auto it = manager.GetAssets().find(entry.guid);
            if (it == manager.GetAssets().end()) return;

            const AssetEntry& assetEntry = it->second;
            if (assetEntry.typeName != entry.typeName) return;

            void* pObject = assetEntry.pObject.get();
            const prop_list* props = assetEntry.pProps;
            if (!pObject || !props) return;

            rapidjson::Document doc;
            doc.Parse(entry.jsonSnapshot.c_str());
            if (doc.HasParseError()) return;

            Deserialize(pObject, *props, doc);
        }
    };
}
#endif // !_UNDO_REDO_SYSTEM_H_