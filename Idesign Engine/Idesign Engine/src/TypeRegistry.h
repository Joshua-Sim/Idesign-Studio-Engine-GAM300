
#pragma once
#ifndef _TYPE_REGISTRY_H_
#define _TYPE_REGISTRY_H_

#include <string>
#include <unordered_map>
#include <functional>
#include "Coordinator.h"

namespace Framework
{
    using TypeGetter = std::function<void* (Coordinator&, Entity)>;

    class TypeRegistry
    {
    public:
        static TypeRegistry& Instance()
        {
            static TypeRegistry instance;
            return instance;
        }

        using TypeAdder = std::function<void(Coordinator&, Entity)>;
        using TypeRemover = std::function<void(Coordinator&, Entity)>;

        template<typename T>
        void Register(const std::string& typeName, const prop_list* props)
        {
            registry[typeName] = [](Coordinator& ecs, Entity e) -> void*
                {
                    if (ecs.HasComponent<T>(e))
                        return &ecs.GetComponent<T>(e);
                    return nullptr;
                };

            adders[typeName] = [](Coordinator& ecs, Entity e)
                {
                    ecs.AddComponent<T>(e, T{});
                };

            removers[typeName] = [](Coordinator& ecs, Entity e)
                {
                    ecs.RemoveComponent<T>(e);
                };

            propLists[typeName] = props; // Link to reflection metadata
        }

        inline const prop_list* GetPropList(const std::string& typeName) const
        {
            auto it = propLists.find(typeName);
            return (it != propLists.end()) ? it->second : nullptr;
        }

        inline bool RemoveComponent(const std::string& typeName, Coordinator& ecs, Entity e)
        {
            auto* remover = GetRemover(typeName);
            if (!remover) return false;
            (*remover)(ecs, e);
            return true;
        }

        inline bool AddComponent(const std::string& typeName, Coordinator& ecs, Entity e)
        {
            auto* adder = GetAdder(typeName);
            if (!adder) return false;
            (*adder)(ecs, e);
            return true;
        }

        inline TypeAdder* GetAdder(const std::string& typeName)
        {
            auto it = adders.find(typeName);
            return (it != adders.end()) ? &it->second : nullptr;
        }

        inline TypeRemover* GetRemover(const std::string& typeName)
        {
            auto it = removers.find(typeName);
            return (it != removers.end()) ? &it->second : nullptr;
        }

        TypeGetter* Get(const std::string& typeName)
        {
            auto it = registry.find(typeName);
            return (it != registry.end()) ? &it->second : nullptr;
        }

        const std::unordered_map<std::string, TypeGetter>& GetAll() const { return registry; }

    private:
        std::unordered_map<std::string, TypeGetter> registry;
        std::unordered_map<std::string, TypeAdder> adders;
        std::unordered_map<std::string, TypeRemover> removers;
        std::unordered_map<std::string, const prop_list*> propLists;
    };
}
#endif // !_TYPE_REGISTRY_H_
