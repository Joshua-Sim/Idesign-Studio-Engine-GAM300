///////////////////////////////////////////////////////////////////////////////
///
///@file ECSystemManager.h
/// 
/// @brief This file contains the definition of the System Manager. It contains
///       functions that allow registration of system, and the setting of
///       the signature of a system.
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#pragma once

#ifndef ECS_SYSTEM_MANAGER_H
#define ECS_SYSTEM_MANAGER_H


#include "System.h"
#include <cassert>
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <any>

namespace Framework {

	class ECSystemManager
	{
	public:
		// ECSystemManager.h
		template<typename T>
		void RegisterSystem(std::shared_ptr<T> system)
		{
			const char* typeName = typeid(T).name();
			assert(mECSystem.find(typeName) == mECSystem.end() && "Registering system more than once!");
			mECSystem.insert({ typeName, std::static_pointer_cast<ISystem>(system) });
		}

		template<typename T> void SetSignature(Signature signature)
		{
			const char* typeName = typeid(T).name();

			assert(mECSystem.find(typeName) != mECSystem.end() && "System used before registered!");

			mSignatures.insert({ typeName, signature });
		}

		/// @brief Clears all registered systems and signatures.
		void Clear()
		{
			// 1. Clear the signature map
			mSignatures.clear();

			// 2. Clear the system map (this destroys all shared_ptrs to systems)
			mECSystem.clear();
		}

		void EntityDestroyed(Entity entity)
		{
			for (auto const& pair : mECSystem)
			{
				auto const& the_system = pair.second;

				the_system->mEntities.erase(entity);
			}
		}

		template<typename T>
		std::shared_ptr<T> GetSystem()
		{
			const char* typeName = typeid(T).name();
			auto it = mECSystem.find(typeName);
			if (it != mECSystem.end())
				return std::static_pointer_cast<T>(it->second);
			return nullptr;
		}


		void EntitySignatureChanged(Entity entity, Signature entitySignature)
		{
			for (auto const& pair : mECSystem)
			{
				auto const& type = pair.first;
				auto const& the_system = pair.second;
				auto const& systemSignature = mSignatures[type];

				if ((entitySignature & systemSignature) == systemSignature)
				{
					the_system->mEntities.insert(entity);
				}

				else
				{
					the_system->mEntities.erase(entity);
				}
			}
		}

		std::vector<Entity> GetAllEntities() const {
			std::vector<Entity> entities;
			for (auto const& pair : mECSystem) {
				for (auto const& e : pair.second->mEntities) {
					if (std::find(entities.begin(), entities.end(), e) == entities.end()) {
						entities.push_back(e);
					}
				}
			}
			return entities;
		}

		template<typename T>
		bool HasComponent(Entity entity) {
			const char* typeName = typeid(T).name();
			auto it = mComponents.find(typeName);
			if (it == mComponents.end()) {
				return false;
			}

			return (it->second.find(entity) != it->second.end());
		}

		template<typename T>
		T& GetComponent(Entity entity) {
			const char* typeName = typeid(T).name();
			auto it = mComponents.find(typeName);
			assert(it != mComponents.end() && "Component type not registered!");
			auto& componentPool = it->second;
			assert(componentPool.find(entity) != componentPool.end() && "Entity does not have this component!");
			return componentPool[entity];
		}

	private:
		std::unordered_map<const char*, Signature> mSignatures{};
		std::unordered_map<const char*, std::shared_ptr<ISystem>> mECSystem{};
		std::unordered_map<const char*, std::unordered_map<Entity, std::any>> mComponents;
	};

}

#endif