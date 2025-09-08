///////////////////////////////////////////////////////////////////////////////////////
//
//	Core.H
//	Authors: Chris Peters, Joshua sim yue chen
// 
// This file defines CoreEngine class that handles the management of systems in Unnamed engine.
// 
//	Copyright 2024, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////////////
#pragma once //Makes sure this header is only included once

#include "System.h"

namespace Framework
{
	///The core manages all the systems in the game. Updating them, routing messages, and
	///destroying them when the game ends.
	class CoreEngine
	{
	public:
		CoreEngine();
		~CoreEngine();
		///Update all the systems until the game is no longer active.
		void GameLoop();
		///Destroy all systems in reverse order that they were added.
		void DestroySystems();
		///Broadcasts a message to all systems.
		//void BroadcastMessage(Message* m);
		///Adds a new system to the game.
		void AddSystem(ISystem* system);
		///Initializes all systems in the game.
		void Initialize();
		void EndGameLoop();
	private:
		//Tracks all the systems the game uses
		std::vector<ISystem*> Systems;
		//The last time the game was updated
		float LastTime;
		//Is the game running (true) or being shut down (false)?
		bool GameActive;
	};

	//A global pointer to the instance of the core
	extern CoreEngine* CORE;
}