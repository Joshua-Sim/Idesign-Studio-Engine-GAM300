///////////////////////////////////////////////////////////////////////////////
//
//	\file MessageSystem.cpp
//	Defines MessageSystem system.
//
//	Authors: Keegan Lim Beng Yang (l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "MessageSystem.h"
#include <iostream>

namespace Framework {

    /********************************************************************
    * MessageSystem: Constructor for the MessageSystem class.           *
    *                Displays a message when the system is created.     *
    ********************************************************************/
    MessageSystem::MessageSystem() {
        std::cout << "MessageSystem created" << std::endl;
    }

    /********************************************************************
    * ~MessageSystem: Destructor for the MessageSystem class.           *
    *                 Displays a message when the system is destroyed.  *
    ********************************************************************/
    MessageSystem::~MessageSystem() {
        std::cout << "MessageSystem destroyed" << std::endl;
    }

    /********************************************************************
    * Initialize: Initializes the MessageSystem.                        *
    *             Displays a message indicating initialization.         *
    ********************************************************************/
    void MessageSystem::Initialize() {
        std::cout << "MessageSystem initialized" << std::endl;
    }

    /********************************************************************
    * Update: This method is called every frame to update the           *
    *         MessageSystem. The deltaTime parameter indicates the      *
    *         time elapsed since the last update.                       *
    *         Currently, this method doesn't perform any specific       *
    *         actions and is left for future per-frame logic.           *
    ********************************************************************/
    void MessageSystem::Update(float deltaTime) {
        (void)deltaTime;
        // Any per-frame logic can go here if needed
        // std::cout << "MessageSystem updating with delta time: " << deltaTime << " seconds" << std::endl;
    }

    /********************************************************************
    * Dispatch: Sends a message/event to all subscribers.               *
    *           - msg: The message/event to be dispatched.              *
    *           Retrieves all handlers subscribed to the message's type *
    *           and executes them by passing the message. Once all      *
    *           handlers have processed the message, the message is     *
    *           deleted to free memory.                                 *
    ********************************************************************/
    void MessageSystem::Dispatch(Message* msg) {
        // Retrieve all handlers subscribed to this type of message
        auto& handlers = subscribers[typeid(*msg)];

        // Execute each handler with the provided message
        for (auto& handler : handlers) {
            handler(msg);
        }

        // Clean up the message/event after dispatching
        delete msg;
    }

    /********************************************************************
    * GetName: Returns the name of the system as a string.              *
    *          Useful for identifying the system within the engine.     *
    ********************************************************************/
    std::string MessageSystem::GetName() {
        return "MessageSystem";
    }

}  // namespace Framework
