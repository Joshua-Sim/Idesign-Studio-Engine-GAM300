///////////////////////////////////////////////////////////////////////////////
///
/// @file ComponentManager.h
/// 
/// @brief Manages the registration and retrieval of component arrays.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include "ComponentArray.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <typeinfo>
#include <cassert>

// Define ComponentType if not already defined globally
using ComponentType = std::uint8_t;

namespace Framework {

    class ComponentManager
    {
    public:
        // Destructor ensures memory is cleared on shutdown
        ~ComponentManager() {
            Clear();
        }

        template<typename T>
        void RegisterComponent()
        {
            // Use string name for safety
            std::string typeName = typeid(T).name();

            assert(mComponentTypes.find(typeName) == mComponentTypes.end() && "Registering component type more than once!");

            mComponentTypes.insert({ typeName, mNextComponentType });

            // Create the specific ComponentArray for this type
            mComponentArrays.insert({ typeName, std::make_shared<ComponentArray<T>>() });

            ++mNextComponentType;
        }

        template<typename T>
        ComponentType GetComponentType()
        {
            std::string typeName = typeid(T).name();
            assert(mComponentTypes.find(typeName) != mComponentTypes.end() && "Component not registered before using!");
            return mComponentTypes[typeName];
        }

        template<typename T>
        void AddComponent(Entity entity, T component)
        {
            GetComponentArray<T>()->InsertData(entity, component);
        }

        template<typename T>
        void RemoveComponent(Entity entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template<typename T>
        bool HasComponent(Entity entity) const
        {
            return GetComponentArray<T>()->HasData(entity);
        }

        template<typename T>
        T& GetComponent(Entity entity)
        {
            return GetComponentArray<T>()->GetData(entity);
        }

        // Defined in ComponentManager.cpp
        void EntityDestroyed(Entity entity);

        void Clear()
        {
            // Clear the maps to release shared_ptrs
            mComponentArrays.clear();
            mComponentTypes.clear();
            mNextComponentType = 0;
        }

    private:
        // Map string names to Component Types (IDs)
        std::unordered_map<std::string, ComponentType> mComponentTypes{};

        // Map string names to the actual Component Storage Arrays
        std::unordered_map<std::string, std::shared_ptr<IComponentArray>> mComponentArrays{};

        ComponentType mNextComponentType{};

        // Helper to get the array for a specific type
        template<typename T>
        std::shared_ptr<ComponentArray<T>> GetComponentArray() const
        {
            std::string typeName = typeid(T).name();

            assert(mComponentTypes.find(typeName) != mComponentTypes.end() && "Component not registered before use!");

            return std::static_pointer_cast<ComponentArray<T>>(mComponentArrays.at(typeName));
        }
    };
}

#endif