///////////////////////////////////////////////////////////////////////////////
///
///@file Coordinator.cpp
/// 
/// @brief This file contains the definition of the non templated functions for
///        the ECS interface to be used in the engine. Functions such as initialize,
///        creating, and destroying of an entity.
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

/* Order of registration of components
MovementComponent 0
EnemyComponent 1 
CollisionComponent 2
TransformComponent 3
RenderComponent 4
LayerComponent 5
TextComponent 6
PlayerComponent 7
*/


#include "pch.h"
#include "Coordinator.h"
#include "ComponentList.h"
#include "AssetHandler.h"
#include "EventSenderComponent.h"
#include "MessageSystem.h"
#include <GameMessages.h>

namespace Framework {

    /* NON TEMPLATED USAGE FUNCTIONS BELOW */
    // ---------------------------- //
    /**
    * @brief Initialises the ECS Interface for usage in main.
    *
    *   The function initialises the respective managers such as the component manager,
    *   entity manager, system manager, and tag manager.
    *
    */
    void Coordinator::Init()
    {
        mComponentManager = std::make_unique<ComponentManager>();
        mEntityManager = std::make_unique<EntityManager>();
        mSystemManager = std::make_unique<ECSystemManager>();
        entityList.clear(); // Clear vector during initialisation of Coordinator
        entityNames.clear(); // Clear entity names during initialisation
    }

    // Made to stop crashes
    void Coordinator::Shutdown()
    {
        if (mComponentManager)
            mComponentManager->Clear();

        if (mSystemManager)
            mSystemManager->Clear(); // Optional if defined

        if (mEntityManager)
            mEntityManager->Clear(); // Optional if defined

        // Clear your containers manually here
        entityNames.clear();
        entityList.clear();
        componentNamesList.clear();
    
    }

    // ---------------------------- //
/**
* @brief Destructor for memory clean-up.
*
*/
    Coordinator::~Coordinator()
    {
        Shutdown();
    }

    /**
    * @brief Creates an Entity.
    *
    *   The function returns an ID from a list of available IDs and
    *   gives disables that ID until the created entity is destroyed.
    *
    */
    Entity Coordinator::CreateEntity()
    {
        Entity newEntity = mEntityManager->CreateEntity();

        SetEntityName(newEntity, "Entity " + std::to_string(newEntity));
        entityList.push_back(newEntity);

        std::string guid = Framework::GenerateGUID();
        guidMap[newEntity] = guid;

        // Default components attaching here (Update eventually)
        
        //Add transform because every entity has x,y,z

        //Add Hierarchy for parent-child and hierarchy management
        return newEntity;
    }

    /**
    * @brief Destroys an entity.
    *
    * The function passes in an entity and destroys it.
    *
    * @param [Entity] entity
    */
    void Coordinator::DestroyEntity(Entity entity)
    {
        entityNames.erase(entity);

        mEntityManager->DestroyEntity(entity);

        mComponentManager->EntityDestroyed(entity);

        mSystemManager->EntityDestroyed(entity);

        guidMap.erase(entity);

        entityList.erase(std::remove(entityList.begin(), entityList.end(), entity), entityList.end());
    }

    /**
    * @brief Gets the Signature of an Entity.
    *
    * The function passes in an entity and returns its signature.
    *
    * @param [Entity] entity
    */
    Signature Coordinator::GetEntitySignature(Entity entity)
    {
        return mEntityManager->GetSignature(entity);
    }

    /**
     * @brief Gets and returns list of entities with a tag.
     *
     * The function passes in an entity and returns its signature.
     *
     * @param [Entity] entity
     */
    std::vector<Entity> Coordinator::GetEntitiesByTag(const std::string& tag) 
    {
        std::vector<Entity> result;

        for (Entity entity : entityList)
        {
            if (HasComponent<TagComponent>(entity))
            {
                const auto& tagComp = GetComponent<TagComponent>(entity);

                // Check if tag exists in the component's tag vector
                for (const std::string& t : tagComp.tags)
                {
                    if (t == tag)
                    {
                        result.push_back(entity);
                        break; // Found the tag, no need to check remaining tags
                    }
                }
            }
        }
        return result;
    }

    /**
 * @brief Adds a tag to an entity's TagComponent
 *
 * If the entity doesn't have a TagComponent, one is added first.
 * If the tag already exists, it won't be added again (no duplicates).
 *
 * @param entity The entity to add the tag to
 * @param tag The tag string to add
 */
    void Coordinator::AddTagToEntity(Entity entity, const std::string& tag)
    {
        // Ensure entity has a TagComponent
        if (!HasComponent<TagComponent>(entity))
        {
            AddComponent<TagComponent>(entity, TagComponent{});
        }

        auto& tagComp = GetComponent<TagComponent>(entity);

        // Check if tag already exists to avoid duplicates
        if (std::find(tagComp.tags.begin(), tagComp.tags.end(), tag) == tagComp.tags.end())
        {
            tagComp.tags.push_back(tag);
        }
    }

    /**
 * @brief Removes a tag from an entity's TagComponent
 *
 * @param entity The entity to remove the tag from
 * @param tag The tag string to remove
 */
    void Coordinator::RemoveTagFromEntity(Entity entity, const std::string& tag)
    {
        if (!HasComponent<TagComponent>(entity))
            return;

        auto& tagComp = GetComponent<TagComponent>(entity);
        auto it = std::find(tagComp.tags.begin(), tagComp.tags.end(), tag);

        if (it != tagComp.tags.end())
        {
            tagComp.tags.erase(it);
        }
    }

    /**
     * @brief Reeturns bool if an entity has a tag
     *
     * The function passes in an entity and tag and returns if entity has the tag.
     *
     * @param [Entity] entity
     */
    bool Coordinator::EntityHasTag(Entity entity, const std::string& tag) 
    {
        if (!HasComponent<TagComponent>(entity))
            return false;

        const auto& tagComp = GetComponent<TagComponent>(entity);
        return std::find(tagComp.tags.begin(), tagComp.tags.end(), tag) != tagComp.tags.end();
    }

    /**
    * @brief Gets the vector of entities in existence
    *
    * The function return the vector of entities.
    *
    */
    const std::vector<Entity>& Coordinator::GetEntities() const
    {
        return entityList;
    }

    void Coordinator::SetEntityName(Entity entity, const std::string& name)
    {
        entityNames[entity] = name;
    }

    Entity Coordinator ::GetEntityByName(const std::string& name)
    {
        for (const auto& [entity, entityName] : entityNames)
        {
            if (entityName == name)
                return entity;
        }
        return -1; // -1 is invalid
    }


    std::string Coordinator::GetEntityName(Entity entity) const
    {
        auto it = entityNames.find(entity);
        return it != entityNames.end() ? it->second : "Unnamed";
    }

    void Coordinator::AddComponentToEntity(Entity entity, const std::string& componentName)
    {
        if (componentName == "MovementComponent")
        {
            mComponentManager->AddComponent(entity, MovementComponent{});

        }
        else if (componentName == "EnemyComponent")
        {
            mComponentManager->AddComponent(entity, EnemyComponent{});
        }

        else if (componentName == "CollisionComponent")
        {
            mComponentManager->AddComponent(entity, CollisionComponent{});
        }

        else if (componentName == "TransformComponent")
        {
            mComponentManager->AddComponent(entity, TransformComponent{});
        }

        else if (componentName == "RenderComponent")
        {
            mComponentManager->AddComponent(entity, RenderComponent{});
        }

        else if (componentName == "LayerComponent")
        {
            mComponentManager->AddComponent(entity, LayerComponent{});
        }

       /* else if (componentName == "TextComponent")
        {
            mComponentManager->AddComponent(entity, TextComponent{});
        }*/

        else if (componentName == "PlayerComponent")
        {
            mComponentManager->AddComponent(entity, PlayerComponent{});
        }
    }

    void Coordinator::ClearEntities()
    {
        // Create a copy of the entity list to safely iterate and destroy entities
        auto entitiesCopy = entityList;

        // Destroy each entity individually
        for (const auto& entity : entitiesCopy)
        {
            DestroyEntity(entity);
        }

        // Clear the entity list and entity names after all entities are destroyed
        entityList.clear();
        entityNames.clear();
        std::cout  << entityList.size();
    }

    bool Coordinator::IsEntityValid(Entity entity) const
    {
        return std::find(entityList.begin(), entityList.end(), entity) != entityList.end();
    }

    std::vector<std::string> Coordinator::GetComponentNames()
    {
        return componentNamesList;
    }

    std::string Coordinator::GetEntityGUID(Entity entity) const
    {
        auto it = guidMap.find(entity);
        return it != guidMap.end() ? it->second : "";
    }

    Entity Coordinator::GetEntityByGUID(const std::string& guid) const
    {
        for (const auto& pair : guidMap)
        {
            if (pair.second == guid)
                return pair.first;
        }
        return -0; // Define INVALID_ENTITY = ~0 if you don't already
        std::cout << "Returning -0 , invalid entity guid retrieved" << std::endl;
    }

    void Coordinator::SetEntityGUID(Entity entity, const std::string& guid)
    {
        guidMap[entity] = guid;
    }


}
