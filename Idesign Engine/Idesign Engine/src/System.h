///////////////////////////////////////////////////////////////////////////////////////
///
///	\file System.h Defines the system interface.
///
///	Authors:  Chris Peters
///	Copyright 2010, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////////////
#pragma once //Makes sure this header is only included once

//#include "Message.h"
#include "Types.h"
#include "set"

namespace Framework
{
	///System is a pure virtual base class (which is to say, an interface) that is
	///the base class for all systems used by the game. 
	///When a system is updated by CoreEngine::GameLoop.
	///  Fixed : 0..N times per frame at a constant 60 Hz (game logic, physics, scripts, ...).
	///  Frame : exactly once per rendered frame with the real frame time (rendering, editor UI).
	enum class UpdateStage { Fixed, Frame };

	class ISystem
	{
	public:
		///Defaults to Fixed. Rendering systems override this to return Frame.
		virtual UpdateStage GetUpdateStage() const { return UpdateStage::Fixed; }

		///Systems can receive all message send to the Core. 
		///See Message.h for details.
		//virtual void SendMessage(Message* message) { UNREFERENCED_PARAMETER(message); };

		///All systems are updated every game frame.
		virtual void Update(float timeslice) = 0;	

		///All systems provide a string name for debugging.
		virtual std::string GetName() = 0;	

		///Initialize the system.
		virtual void Initialize(){};
		
		///All systems need a virtual destructor to have their destructor called 
		virtual ~ISystem(){}

		// For ECS System to hold a set of entities within each respective system
		std::set<Entity> mEntities;	
	};
}
