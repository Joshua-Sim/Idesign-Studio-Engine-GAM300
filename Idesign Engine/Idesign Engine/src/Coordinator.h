///////////////////////////////////////////////////////////////////////////////
///
///@file Coordinator.h
/// 
/// @brief This file contains the definition of the templated functions for
///        the ECS interface to be used in the engine. Functions that handle
///       different components and registration of different systems are defined
///       here.
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once

#ifndef COORDINATOR_H
#define COORDINATOR_H

#include "ComponentManager.h"
#include "EntityManager.h"
#include "ECSystemManager.h"
#include "EventSenderComponent.h"
#include "TagManager.h"
#include "Types.h"
#include <memory>

#include <functional>

namespace Framework {

	class Coordinator
	{
	public:

		~Coordinator();

		void Init();

		// To Stop Crashes
		void Shutdown();

		// Entity Functions
		Entity CreateEntity();

		void DestroyEntity(Entity entity);

		Signature GetEntitySignature(Entity entity); // Testing stuff


		/*	ALL TEMPLATED USAGE FUNCTIONS */
		// Component Functions
		/**
		* @brief Registers a component.
		*
		* The function template registers a component to the array of components, handled
		* by the component manager.
		*
		*/
		template<typename T> void RegisterComponent()
		{
			mComponentManager->RegisterComponent<T>();
		}

		/**
		* @brief Adds a component to an entity.
		*
		* The function takes in an entity and a component, and
		* then adds the said component into the entity.
		*
		* @param [Entity] entity
		* @param [T] component
		*/
		template<typename T> void AddComponent(Entity entity, T component)
		{
			mComponentManager->AddComponent<T>(entity, component);

			auto signature = mEntityManager->GetSignature(entity);
			signature.set(mComponentManager->GetComponentType<T>(), true);
			mEntityManager->SetSignature(entity, signature);

			mSystemManager->EntitySignatureChanged(entity, signature);
		}

		/**
		* @brief Removes a component from an entity.
		*
		* The function takes in an entity and removes the entity
		* from that component type invoked.
		*
		* @param [Entity] entity
		*/
		template<typename T> void RemoveComponent(Entity entity)
		{
			mComponentManager->RemoveComponent<T>(entity);

			auto signature = mEntityManager->GetSignature(entity);
			signature.set(mComponentManager->GetComponentType<T>(), false);
			mEntityManager->SetSignature(entity, signature);

			mSystemManager->EntitySignatureChanged(entity, signature);
		}

		/**
		* @brief Gets a component from an entity.
		*
		* The function takes in an entity and returns the component
		* from the component type invoked.
		*
		* @param [Entity] entity
		*/
		template<typename T> T& GetComponent(Entity entity)
		{
			return mComponentManager->GetComponent<T>(entity);
		}

		/**
		* @brief Gets the index of a component in the component type array.
		*
		* The function returns the index of a component in the component type
		* array from the component invoked.
		*
		*/
		template<typename T> ComponentType GetComponentType()
		{
			return mComponentManager->GetComponentType<T>();
		}

		/**
		* @brief Gets the index of a component in the component type array and check for existing component.
		*
		* The function returns the bool if a component exists in the entity index.
		*
		*/
		template<typename T> bool HasComponent(Entity entity) const
		{
			return mComponentManager->HasComponent<T>(entity);
		}

		/**
	* @brief Clears all entities.
	*
	* The function iterates through all entities in the entity list,
	* destroys each one, and then clears the list.
	*/
		void ClearEntities();

		// System Functions

		/**
		* @brief Registers a system to a list of systems.
		*
		* The function registers a system to a list of systems
		* based on the system the function is invoked from.
		*
		*/
		template<typename T, typename... Args>
		std::shared_ptr<T> RegisterSystem(Args&&... args)
		{
			//static_assert(std::is_constructible<T, Args...>::value, "T must be constructible with provided arguments");

			// Create the system instance with arguments (if any)
			auto system = std::make_shared<T>(std::forward<Args>(args)...);

			// Register the already-created system instance
			mSystemManager->RegisterSystem(system);
			return system;
		}

		/**
		* @brief Sets the signature of a system.
		*
		* The function takes in a signature, and then sets the signature
		* of the system to that signature passed in, based on the system that the
		* function is invoked from.
		*
		* @param [Entity] entity
		*/
		template<typename T> void SetSystemSignature(Signature signature)
		{
			mSystemManager->SetSignature<T>(signature);
		}

		// Tag Managing Functions
		// Takes in an entity and a tag name, then assigns a tag to the entity
		void AddTag(Entity entity, const std::string& tag);

		// Takes in an entity and a tag name, then removes the tag from that entity
		void RemoveTag(Entity entity, const std::string& tag);

		// Takes in an entity and a tag name, then returns true if the tag exists, or false if it does not
		bool HasTag(Entity entity, const std::string& tag) const;

		// Takes in a tag name, and returns the entities that contain said tag
		const std::unordered_set<Entity>& GetEntitiesByTag(const std::string& tag) const;

		// Takes in an entity and removes all tags from it 
		void RemoveAllTags(Entity entity);

		// Entity Vector Stuff
		// Returns the vector of all entities in existence
		const std::vector<Entity>& GetEntities() const;

		// Entity Name Stuff
		// Takes in an entity and a string, setting the name of the entity to be the string
		void SetEntityName(Entity entity, const std::string& name);

		Entity GetEntityByName(const std::string& name);

		// Returns the name of the given entity
		std::string GetEntityName(Entity entity) const;

		// Add Component to Entity via IMGUI. Used in Graphics.cpp
		void AddComponentToEntity(Entity entity, const std::string& componentName);

		// Checks if the entity is alive in the list of entities
		bool IsEntityValid(Entity entity) const;

		// Returns a vector of the components in string
		std::vector<std::string> GetComponentNames();

		// ------ GUID functions -----

		std::string GetEntityGUID(Entity entity) const;

		Entity GetEntityByGUID(const std::string& guid) const;

		void SetEntityGUID(Entity entity, const std::string& guid);
		
		// System retreival
		template<typename T>
		std::shared_ptr<T> GetSystem()
		{
			return mSystemManager->GetSystem<T>();
		}

		// Edwin's Stuff (For Auto Generate Entity Inspector)
		using RawGetter = std::function<void* (Entity)>;

		void RegisterRawGetter(const std::string& typeName, RawGetter getter)
		{
			rawGetters[typeName] = std::move(getter);
		}

		void* GetRawComponent(Entity entity, const std::string& typeName)
		{
			//std::cout << "Fetching component: " << typeName << std::endl;

			auto it = rawGetters.find(typeName);
			if (it != rawGetters.end())
				return it->second(entity);
			return nullptr;
		}

		std::unordered_map<std::string, void*> GetAllComponents(Entity entity)
		{
			std::unordered_map<std::string, void*> result;

			for (const auto& [typeName, getter] : rawGetters)
			{
				void* componentPtr = getter(entity);
				if (componentPtr)
				{
					result[typeName] = componentPtr;
				}
			}
			return result;
		}


	private:
		std::unique_ptr<ComponentManager> mComponentManager;	// A shared pointer of the Component Manager
		std::unique_ptr<EntityManager> mEntityManager;			// A shared pointer of the Entity Manager
		std::unique_ptr<ECSystemManager> mSystemManager;		// A shared pointer of the ECS System Manager
		std::unique_ptr<TagManager> mTagManager;				// A shared pointer of the Tag Manager
		std::unordered_map<Entity, std::string> entityNames;	// An unordered map of entity names, using the entity IDs as their keys. Since IDs are unique
		std::unordered_map<Entity, std::string> guidMap;        // Unordered map of entity GUID to entity ID linkage, generated uniquely on creation.
		std::vector<Entity> entityList;							// A vector of entities used for tracking all entities in game
		std::vector<std::string> componentNamesList =			// A vector of strings that tracks the names of components. Need to edit this
		{ "MovementComponent", "EnemyComponent", "CollisionComponent",
			"TransformComponent", "RenderComponent", "LayerComponent",
			"TextComponent", "PlayerComponent" }; 

		std::unordered_map<std::string, RawGetter> rawGetters;
	};
}
#endif