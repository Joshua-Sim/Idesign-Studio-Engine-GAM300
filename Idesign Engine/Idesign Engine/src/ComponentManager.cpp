///////////////////////////////////////////////////////////////////////////////
///
///@file ComponentManager.cpp
/// 
/// @brief This file contains the defintion of a the EntityDestroyed function
///       under the component manager
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#include "pch.h"
#include "ComponentManager.h"

namespace Framework 
{
	// Passes in an entity and notifies all component arrays that a specific entity has been destroyed, then freeing it from the array
	void ComponentManager::EntityDestroyed(Entity entity)
	{
		for (auto const& pair : mComponentArrays)
		{
			auto const& component = pair.second;

			component->EntityDestroyed(entity);
		}
	}
}