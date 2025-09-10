#pragma once
///////////////////////////////////////////////////////////////////////////////
///
/// @file LogicManager.h
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

#pragma once //Makes sure this header is only included once

#include "pch.h"
#include "System.h"
#include "Vector2D.h"
#include "InputHandler.h"
#include "ComponentList.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <types.h>

// Define the function type for behaviors (e.g., taking an Entity and delta time as arguments)
using BehaviorFunction = std::function<void(Framework::Entity, float)>;

class LogicManager {
public:
    // Register a behavior function with a name
    void RegisterFunction(const std::string& name, BehaviorFunction function);

    // Retrieve a function by name
    BehaviorFunction GetFunction(const std::string& name) const;

    void Initialize(); // Add an Init function to initialize all behaviors

    void InitializeEnemy(Framework::Entity entity);

private:
    // Registry mapping function names to behavior functions
    std::unordered_map<std::string, BehaviorFunction> functionRegistry;
};

// Global access to the LogicManager instance
extern LogicManager GlobalLogicManager;


