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

		template<typename T>
		std::shared_ptr<T> GetSystem()
		{
			const char* typeName = typeid(T).name();
			auto it = mECSystem.find(typeName);
			if (it == mECSystem.end())
			{
				assert(false && "System not found! Did you forget to register it?");
				return nullptr;
			}
			return std::static_pointer_cast<T>(it->second);
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

	private:
		std::unordered_map<const char*, Signature> mSignatures{};
		std::unordered_map<const char*, std::shared_ptr<ISystem>> mECSystem{};
	};

}

#endif