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
        mTagManager = std::make_unique<TagManager>();
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

        if (mTagManager)
            mTagManager->Clear();   // Optional if defined
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

        mTagManager->RemoveAllTags(entity);

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
    * @brief Adds a tag for grouping Entities.
    *
    * The function passes in an entity and a tag, then assigns a tag to the entity.
    *
    * @param [Entity] entity
    * @param [const std::string&] tag
    */
    void Coordinator::AddTag(Entity entity, const std::string& tag)
    {
        mTagManager->AddTag(entity, tag);
    }

    /**
    * @brief Removes a tag from an Entity.
    *
    * The function passes in an entity and a tag, then removes the tag from the entity.
    *
    * @param [Entity] entity
    * @param [const std::string&] tag
    */
    void Coordinator::RemoveTag(Entity entity, const std::string& tag)
    {
        mTagManager->RemoveTag(entity, tag);
    }

    /**
    * @brief Checks if an entity has a specific tag.
    *
    * The function passes in an entity and a tag, then returns true
    * if the tag is assigned to the entity.
    *
    * @param [Entity] entity
    * @param [const std::string&] tag
    */
    bool Coordinator::HasTag(Entity entity, const std::string& tag) const
    {
        return mTagManager->HasTag(entity, tag);
    }

    /**
    * @brief Gets all entities with a specific tag.
    *
    * The function takes in a tag, then returns an unordered set of
    * entities that has the tag
    *
    * @param [const std::string&] tag
    */
    const std::unordered_set<Entity>& Coordinator::GetEntitiesByTag(const std::string& tag) const
    {
        return mTagManager->GetEntitiesByTag(tag);
    }

    /**
    * @brief Removes all tags from an entity.
    *
    * The function takes in a tag, then returns an unordered set of
    * entities that has the tag.
    *
    * @param [Entity] entity
    */
    void Coordinator::RemoveAllTags(Entity entity)
    {
        mTagManager->RemoveAllTags(entity);
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

    // Event System
    void Coordinator::FireTrigger(Entity entity, Entity otherEntity, EEventTriggerType triggerType)
    {
        if (!mComponentManager->HasComponent<EventSenderComponent>(entity)) {
            return;
        }

        auto& sender = mComponentManager->GetComponent<EventSenderComponent>(entity);

        // Compare the event that *just happened* (triggerType)
        // with the event the component is *waiting for* (sender.onTrigger).
        if (static_cast<int>(sender.onTrigger) == static_cast<int>(triggerType)) {

            // --- This block is the mapping ---
            if (sender.messageName == "PlayerInteractedWithItem") {

                MessageBus::Get().QueueMessage(
                    PlayerInteractedWithItem{ otherEntity, entity }
                );
            }

            else if (sender.messageName == "click") {

                MessageBus::Get().QueueMessage(
                    EntityClickedMessage{ otherEntity, entity }
                );
            }
        }
    }

}
