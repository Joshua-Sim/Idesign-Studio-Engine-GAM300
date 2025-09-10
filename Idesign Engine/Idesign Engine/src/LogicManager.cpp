///////////////////////////////////////////////////////////////////////////////
///
/// @file LogicManager.cpp
/// 
/// @brief This file contains the implementation of the LogicManager class,
///        which manages and registers behavior functions for entities in the ECS.
///        The LogicManager allows for flexible assignment of behavior functions to
///        entities, enabling dynamic updates of entity behaviors at runtime.
///
/// @authors Dylan Lau
/// @copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "AnimationSystem.h"
#include <iostream>
#include "Coordinator.h"
#include "InputHandler.h"
#include "FontSystem.h" 
#include "EngineState.h"
#include "LogicManager.h"
#include <iostream> // Optional: For logging or debugging
#include "EnemyBehavior.h"


extern Framework::Coordinator ecsInterface;
// Define the global LogicManager instance
LogicManager GlobalLogicManager;

void LogicManager::Initialize() {
    // Register the behavior functions here
    RegisterEnemyBehaviors();
}

void LogicManager::RegisterFunction(const std::string& name, BehaviorFunction function) {
    if (functionRegistry.find(name) == functionRegistry.end()) {
        functionRegistry[name] = function;
    }
    else {
        std::cerr << "Warning: Function '" << name << "' is already registered." << std::endl;
    }
}

BehaviorFunction LogicManager::GetFunction(const std::string& name) const {
    auto it = functionRegistry.find(name);
    if (it != functionRegistry.end()) {
        return it->second;
    }
    else {
        std::cerr << "Error: Function '" << name << "' not found in LogicManager." << std::endl;
        return nullptr;
    }
}

//Parse this to reallocate function
void LogicManager::InitializeEnemy(Framework::Entity entity) {
    // Ensure the entity has an EnemyComponent
    if (!ecsInterface.HasComponent<EnemyComponent>(entity)) {
        std::cerr << "Error: Entity " << entity << " does not have an EnemyComponent." << std::endl;
        return;
    }

    // Retrieve the EnemyComponent and function name
    auto& enemyComponent = ecsInterface.GetComponent<EnemyComponent>(entity);
    BehaviorFunction behaviorFunction = GetFunction(enemyComponent.UpdateFunctionName);

    if (behaviorFunction) {
        enemyComponent.behavior = behaviorFunction;
        std::cout << "Assigned behavior '" << enemyComponent.UpdateFunctionName << "' to entity " << entity << std::endl;
    }
    else {
        std::cerr << "Error: Behavior function '" << enemyComponent.UpdateFunctionName
            << "' not found for entity " << entity << std::endl;
    }
}
