///////////////////////////////////////////////////////////////////////////////
///
///@file ComponentManager.h
/// 
/// @brief This file contains the defintion of a component manager and 
///       how it manages the different components and their arrays
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef COMPONENT_MANAGER_H
#define COMPONENT_MANAGER_H

#include "ComponentArray.h"
#include <memory>

namespace Framework {

	class ComponentManager
	{
	public:
		// Registers a component into the Component Array based on the component type.
		template<typename T> void RegisterComponent()
		{
			const char* typeName = typeid(T).name();

			assert(mComponentTypes.find(typeName) == mComponentTypes.end() && "Registering component type more than once!");

			mComponentTypes.insert({ typeName, mNextComponentType });

			mComponentArrays.insert({ typeName, std::make_shared<ComponentArray<T>>() });

			++mNextComponentType;
		}

		// Returns the component based on the name of the component
		template<typename T> ComponentType GetComponentType()
		{
			const char* typeName = typeid(T).name();

			assert(mComponentTypes.find(typeName) != mComponentTypes.end() && "Component not registered before using!");

			return mComponentTypes[typeName];
		}

		// Passes in an entity and a component, then adds said component into said entity
		template<typename T> void AddComponent(Entity entity, T component)
		{
			GetComponentArray<T>()->InsertData(entity, component);
		}

		// Passes in an entity and removes the entity from the array of entities in that said component
		template<typename T> void RemoveComponent(Entity entity)
		{
			GetComponentArray<T>()->RemoveData(entity);
		}

		template<typename T> bool HasComponent(Entity entity) const
		{
			auto componentArray = GetComponentArray<T>();
			return componentArray->HasData(entity);  // Assumes HasData checks if entity has that component
		}

		// Passes in an entity and returns a reference to the component of the enttity from that component array
		template<typename T> T& GetComponent(Entity entity)
		{
			return GetComponentArray<T>()->GetData(entity);
		}

		void EntityDestroyed(Entity entity);

		void Clear()
		{
			mComponentArrays.clear();  // releases all shared_ptrs to ComponentArray<T>
			mComponentTypes.clear();   // clears type-name to type-index map
			mNextComponentType = 0;    // reset counter (optional)
		}

	private:
		std::unordered_map<const char*, ComponentType> mComponentTypes{};						// An unordered map containing the index for the Components registered in the system
		std::unordered_map<const char*, std::shared_ptr<IComponentArray>> mComponentArrays{};	// An unordered map containing the components in the array of components
		ComponentType mNextComponentType{};														// A ComponentType (unsigned int) tracking number for the index of component types per registration

		// Used by the component manager for managing of the components for the specified components
		template<typename T>
		std::shared_ptr<ComponentArray<T>> GetComponentArray() const
		{
			const char* typeName = typeid(T).name();

			assert(mComponentTypes.find(typeName) != mComponentTypes.end() && "Component not registered before use!");

			return std::static_pointer_cast<ComponentArray<T>>(mComponentArrays.at(typeName));
		}

	};
}

#endif
