///////////////////////////////////////////////////////////////////////////////////////
//
//	Core.cpp
//	Authors: Chris Peters, Joshua Sim, Keegan Lim
// 
// This file defines CoreEngine class that handles the management of systems in Unnamed engine.
// 
//	Copyright 2024, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "Core.h"
#include "EngineState.h"
#include "Debugger.h"
#include <chrono>
namespace Framework
{
	//A global pointer to the core
	CoreEngine* CORE;

	extern Framework::EngineState engineState;
	/**
 * \brief CoreEngine constructor.
 *
 * Initializes the CoreEngine, sets the initial time to 0, and marks the game as active.
 * Also sets the global CORE pointer to this instance.
 */
	CoreEngine::CoreEngine()
	{
		LastTime = 0;
		GameActive = true;
		CORE = this; //Set the global pointer
	}

	/**
 * \brief CoreEngine destructor.
 *
 * The destructor doesn't perform any actions since no dynamic resources are directly managed.
 */
	CoreEngine::~CoreEngine()
	{
		//Nothing for the destructor to do
	}

	/**
	 * \brief Initializes all systems added to the CoreEngine.
	 *
	 * Calls the Initialize() method on each system in the order they were added.
	 */
	void CoreEngine::Initialize()
	{
		for (unsigned i = 0; i < Systems.size(); ++i)
			Systems[i]->Initialize();
	}

	/**
* \brief The main game loop.
*
* This loop continuously runs while the game is active. It calculates the delta time (dt),
* which represents the time passed since the last frame, and updates all systems using this dt.
* It continues until the game is marked as inactive (GameActive is set to false).
*/
	void CoreEngine::GameLoop()
	{
		LastTime = static_cast<float>(glfwGetTime());
		DebugSystem* debugSystem = DebugSystem::GetInstance();

		while (GameActive)
		{
			double currentTime = glfwGetTime();
			float dt = static_cast<float>(currentTime - LastTime);
			LastTime = static_cast<float>(currentTime);

			std::unordered_map<std::string, double> systemTimes;

			for (auto* system : Systems) {
				auto start = std::chrono::high_resolution_clock::now();

				if (engineState.IsPlay()) {
					system->Update(dt);
				}
				else{
					system->Update(0);
				}
			
				auto end = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double, std::milli> elapsed = end - start;

				systemTimes[system->GetName()] = elapsed.count();
			}

			// Update DebugSystem with timing data
			if (debugSystem) {
				debugSystem->SetSystemTimes(systemTimes);
				debugSystem->ProcessSystemUpdateTimes(dt);
			}
		}
	}



	
	/**
 * \brief Adds a system to the CoreEngine.
 *
 * Adds a new system to the list of systems managed by the CoreEngine. These systems
 * will be updated every frame in the game loop.
 *
 * \param system Pointer to the system to be added to the CoreEngine.
 */
	void CoreEngine::AddSystem(ISystem* system)
	{
		//Add a system to the core to be updated
		//every frame
		Systems.push_back(system);
	}

	/**
 * \brief Destroys and deallocates all systems.
 *
 * Deletes all systems in reverse order of their addition to minimize
 * any dependency issues between systems. After this function, all systems are removed.
 */
void CoreEngine::DestroySystems()
{
    // Just clear the vector; smart pointers handle the memory cleanup automatically
    Systems.clear();
}



	/**
 * \brief Ends the game loop.
 *
 * Sets the GameActive flag to false, causing the game loop to stop on the next iteration.
 */
	void CoreEngine::EndGameLoop() {
		GameActive = false;
	}
}
