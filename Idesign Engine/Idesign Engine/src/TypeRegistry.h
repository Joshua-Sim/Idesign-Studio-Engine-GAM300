
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

        template<typename T>
        void Register(const std::string& typeName)
        {
            registry[typeName] = [](Coordinator& ecs, Entity e) -> void*
                {
                    if (ecs.HasComponent<T>(e))
                        return &ecs.GetComponent<T>(e);
                    return nullptr;
                };
        }

        TypeGetter* Get(const std::string& typeName)
        {
            auto it = registry.find(typeName);
            return (it != registry.end()) ? &it->second : nullptr;
        }

        const std::unordered_map<std::string, TypeGetter>& GetAll() const { return registry; }

    private:
        std::unordered_map<std::string, TypeGetter> registry;
    };
}
#endif // !_TYPE_REGISTRY_H_
