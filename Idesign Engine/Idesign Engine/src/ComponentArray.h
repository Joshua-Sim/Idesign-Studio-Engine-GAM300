/////////////////////////////////////////////////////////////////////////////// 
/// 
///@file ComponentArray.h 
///  
/// @brief This file contains a base class of an interface of a Component Array, and the 
///       implementation of a Component Array that contains the entities with any specified component 
///   
///@Authors: Teo Joel 
///Copyright 2024, Digipen Institute of Technology 
/// 
/////////////////////////////////////////////////////////////////////////////// 

#pragma once 

#ifndef COMPONENT_ARRAY_H 
#define COMPONENT_ARRAY_H 

#include "Types.h" 
#include <array> 
#include <cassert> 
#include <unordered_map> 

/*
 An interface of ComponentArray class (declared further below) is
 created so that the component array can handle any type of components
 when we define them later in our project.

 The use of virtual functions mean that each type of component
 must provide its own definition of the functions.
*/

namespace Framework
{
    // IComponentArray for inheritance for Component Arrays that allow for overriding of functions 
    class IComponentArray
    {
    public:
        virtual ~IComponentArray() = default;
        virtual void EntityDestroyed(Entity entity) = 0;
    };

    template<typename T> class ComponentArray : public IComponentArray
    {
    public:
        // Inserts a component into the entity 
        void InsertData(Entity entity, T component)
        {
            assert(mEntityToIndexMap.find(entity) == mEntityToIndexMap.end() && "Component added to same entity more than once!");

            // Places a new entry at the end 
            size_t newIndex = mSize;
            mEntityToIndexMap[entity] = newIndex;
            mIndexToEntityMap[newIndex] = entity;
            mComponentArray[newIndex] = component;
            ++mSize;
        }

        // Removes the component associated with the entity passed in 
        void RemoveData(Entity entity)
        {
            //auto it = mEntityToIndexMap.find(entity);
            //if (it == mEntityToIndexMap.end() || mSize == 0)
            //    return; // entity doesn't have this component (or nothing stored)

            //size_t indexOfRemovedEntity = it->second;
            //size_t indexOfLastElement = mSize - 1;

            //// Move last element into removed slot (faster + safer for vectors)
            //mComponentArray[indexOfRemovedEntity] = std::move(mComponentArray[indexOfLastElement]);

            //Entity entityOfLastElement = mIndexToEntityMap[indexOfLastElement];
            //mEntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
            //mIndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

            //mEntityToIndexMap.erase(entity);
            //mIndexToEntityMap.erase(indexOfLastElement);

            //--mSize;

            auto it = mEntityToIndexMap.find(entity);
            if (it == mEntityToIndexMap.end() || mSize == 0)
                return; // removing non-existent component (safe no-op)

            size_t indexOfRemovedEntity = it->second;
            size_t indexOfLastElement = mSize - 1;

            mComponentArray[indexOfRemovedEntity] = std::move(mComponentArray[indexOfLastElement]);

            Entity entityOfLastElement = mIndexToEntityMap[indexOfLastElement];
            mEntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
            mIndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

            mEntityToIndexMap.erase(entity);
            mIndexToEntityMap.erase(indexOfLastElement);

            --mSize;
        }

        // Returns a reference to the entity's component 
        T& GetData(Entity entity)
        {
            //assert(mEntityToIndexMap.find(entity) != mEntityToIndexMap.end() && "Retrieving non-existent component!");

            //return mComponentArray[mEntityToIndexMap[entity]]; // Return a reference to the entity's component 

            auto it = mEntityToIndexMap.find(entity);
            assert(it != mEntityToIndexMap.end() && "Retrieving non-existent component!");

            return mComponentArray[it->second];
        }

        // Removes the entity's component. Overrides from the base class to call this function for any T type component 
        void EntityDestroyed(Entity entity) override
        {
            if (mEntityToIndexMap.find(entity) != mEntityToIndexMap.end())
            {
                RemoveData(entity); // Remove the entity's component if it existed 
            }
        }

        // Add this function inside the ComponentArray class 
        bool HasData(Entity entity) const
        {
            // Check if the entity exists in the mEntityToIndexMap 
            return mEntityToIndexMap.find(entity) != mEntityToIndexMap.end();
        }

    private:
        std::array<T, MAX_ENTITIES> mComponentArray{};
        /*
         The packed array of components (of generic type T)
         set to a specified maximum amount, matching the maximum num
         of entities allowed to exist simultaneously, so that each
         entity has a unique spot in the array.
        */
        std::unordered_map<Entity, size_t> mEntityToIndexMap{};  // Map from an entity ID to an array index 
        std::unordered_map < size_t, Entity> mIndexToEntityMap{};  // Map from an array index to an entity ID 
        size_t mSize{};            // Total size of valid entries in the array 
    };
}

#endif