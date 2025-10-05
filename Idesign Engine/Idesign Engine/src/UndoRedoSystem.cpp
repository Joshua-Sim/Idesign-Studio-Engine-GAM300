
#include "pch.h"
#include "UndoRedoSystem.h"

extern Framework::Coordinator ecsInterface;
namespace Framework
{
    void UndoRedoManager::PushAssetUndo(const AssetHandle& guid, const std::string& typeName, void* pObject, const prop_list& props)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();
        Serialize(pObject, props, doc, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        undoStack.push_back({ guid, typeName, buffer.GetString() });
        redoStack.clear();

        if (undoStack.size() > MAX_UNDO_REDO)
            undoStack.erase(undoStack.begin());
    }

    void UndoRedoManager::PushComponentUndo(Entity entity, const std::string& typeName, void* pComponent, const prop_list& props)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();
        Serialize(pComponent, props, doc, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        ecsUndoStack.push_back({ entity, typeName, buffer.GetString() });
        ecsRedoStack.clear();

        if (ecsUndoStack.size() > MAX_UNDO_REDO)
            ecsUndoStack.erase(ecsUndoStack.begin());
    }
	
    void UndoRedoManager::UndoAsset()
    {
        if (undoStack.empty()) return;
        auto entry = undoStack.back();
        undoStack.pop_back();
        redoStack.push_back(entry);
        ApplyAssetSnapshot(entry);
    }

    void UndoRedoManager::RedoAsset()
    {
        if (redoStack.empty()) return;
        auto entry = redoStack.back();
        redoStack.pop_back();
        undoStack.push_back(entry);
        ApplyAssetSnapshot(entry);
    }

    void UndoRedoManager::UndoComponent()
    {
        if (ecsUndoStack.empty()) return;
        auto entry = ecsUndoStack.back();
        ecsUndoStack.pop_back();
        ecsRedoStack.push_back(entry);
        ApplyComponentSnapshot(entry);
    }

    void UndoRedoManager::RedoComponent()
    {
        if (ecsRedoStack.empty()) return;
        auto entry = ecsRedoStack.back();
        ecsRedoStack.pop_back();
        ecsUndoStack.push_back(entry);
        ApplyComponentSnapshot(entry);
    }

    void UndoRedoManager::Clear()
    {
        undoStack.clear();
        redoStack.clear();
        ecsUndoStack.clear();
        ecsRedoStack.clear();
    }

	
    void UndoRedoManager::ApplyAssetSnapshot(const AssetUndoEntry& entry)
    {
        auto& manager = AssetManager::Instance();
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
        manager.MarkDirty(entry.guid);
    }

    void UndoRedoManager::ApplyComponentSnapshot(const ComponentUndoEntry& entry)
    {
        //auto* pComponent = ecsInterface.GetComponent(entry.entity, entry.componentType);
        //const prop_list* props = FindPropListByName(entry.componentType);
        //if (!pComponent || !props) return;

        rapidjson::Document doc;
        doc.Parse(entry.jsonSnapshot.c_str());
        if (doc.HasParseError()) return;

        //Deserialize(pComponent, *props, doc);
        //AssetManager::Instance().MarkEntityDirty(entry.entity); // optional
    }
}