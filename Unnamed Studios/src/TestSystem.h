///////////////////////////////////////////////////////////////////////////////
//
//	\file TestSystem.h
//	Defines TestSystem system.
//
//	Authors: Keegan Lim Beng Yang (l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "System.h"
#include "MessageSystem.h"
#include <iostream>

namespace Framework {

    /********************************************************************
    * TestSystem: A system that listens for messages from the           *
    *             MessageSystem and responds to specific events         *
    *             (e.g., key presses, mouse clicks, mouse movements).   *
    ********************************************************************/
    class TestSystem : public ISystem {
    public:
        /********************************************************************
        * TestSystem: Constructor that initializes the TestSystem and       *
        *             subscribes to relevant messages in the MessageSystem. *
        *             - messageSystem: A pointer to the MessageSystem used  *
        *                              for subscribing to messages/events.  *
        ********************************************************************/
        TestSystem(MessageSystem* messageSystem);

        /********************************************************************
        * Initialize: Initializes the TestSystem.                          *
        ********************************************************************/
        void Initialize() override;

        /********************************************************************
        * Update: The update method for TestSystem, executed every frame.   *
        *         - deltaTime: Time elapsed since the last frame.           *
        ********************************************************************/
        void Update(float deltaTime) override;

        /********************************************************************
        * GetName: Returns the name of the system.                          *
        *         Used mainly for identification and debugging.             *
        ********************************************************************/
        std::string GetName() override { return "TestSystem"; }

        /********************************************************************
        * OnKeyPressed: Handles the MessageKeyPressed event when a key is   *
        *               pressed.                                            *
        *               - msg: Pointer to the key press message.            *
        ********************************************************************/
        void OnKeyPressed(MessageKeyPressed* msg);

        /********************************************************************
        * OnMouseClick: Handles the MessageMouseClick event when a mouse    *
        *               button is clicked.                                  *
        *               - msg: Pointer to the mouse click message.          *
        ********************************************************************/
        void OnMouseClick(MessageMouseClick* msg);

        /********************************************************************
        * OnMouseMoved: Handles the MessageMouseMove event when the mouse   *
        *               is moved.                                           *
        *               - msg: Pointer to the mouse movement message.       *
        ********************************************************************/
        void OnMouseMoved(MessageMouseMove* msg);

    private:
        MessageSystem* messageSystem;  // Pointer to the MessageSystem instance
    };

}  // namespace Framework

