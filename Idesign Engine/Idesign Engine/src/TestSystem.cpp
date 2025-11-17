///////////////////////////////////////////////////////////////////////////////
//
//	\file TestSystem.cpp
//	Defines MessageSystem system.
//
//	Authors: Keegan Lim Beng Yang (l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "TestSystem.h"
#include "ImguiManager.h"


namespace Framework {

    /********************************************************************
    * TestSystem: Constructor for the TestSystem class.                 *
    *             Subscribes to various message types from the          *
    *             MessageSystem upon initialization.                    *
    *             - messageSystem: Pointer to the MessageSystem used    *
    *                              for subscribing to messages.         *
    ********************************************************************/
    TestSystem::TestSystem(MessageSystem* messageSystem) : messageSystem(messageSystem) {
        // Subscribe to MessageKeyPressed
        messageSystem->Subscribe<MessageKeyPressed>([this](MessageKeyPressed* msg) {
            OnKeyPressed(msg);
            });

        // Subscribe to MessageMouseClick
        messageSystem->Subscribe<MessageMouseClick>([this](MessageMouseClick* msg) {
            OnMouseClick(msg);
            });

        // Subscribe to mouse move messages
        messageSystem->Subscribe<MessageMouseMove>([this](MessageMouseMove* msg) {
            OnMouseMoved(msg);
            });
    }

    /********************************************************************
    * Initialize: Initializes the TestSystem.                           *
    *             Prints a confirmation message when initialized.       *
    ********************************************************************/
    void TestSystem::Initialize() {
        std::cout << "TestSystem initialized" << std::endl;
		ImguiManager::LogSuccess("TestSystem initialized", "TestSystem");
    }

    /********************************************************************
    * Update: The update method for the TestSystem, which runs every    *
    *         frame. Currently, there is no frame-specific logic here.  *
    *         - deltaTime: Time elapsed since the last frame.           *
    ********************************************************************/
    void TestSystem::Update(float deltaTime) {
        (void)deltaTime;
        // No need to do anything here for now
    }

    /********************************************************************
    * OnKeyPressed: Handles the MessageKeyPressed event when a key is   *
    *               pressed. Ensures that key press messages are only   *
    *               processed once, even if the key is held down.       *
    *               - msg: Pointer to the key press message.            *
    ********************************************************************/
    void TestSystem::OnKeyPressed(MessageKeyPressed* msg) {
        static std::unordered_map<int, bool> keyHandled;  // Track if a key is being held

        // Only process if it's the first press
        if (!keyHandled[msg->keyCode]) {
            std::cout << "TestSystem received key press message: " << msg->keyCode << std::endl;
			ImguiManager::Log("TestSystem received key press message: " + std::to_string(msg->keyCode), "TestSystem");
            keyHandled[msg->keyCode] = true;  // Mark the key as handled
        }

        // Reset the state when the key is released (you should add a handler for key release in InputHandler)
        if (msg->keyCode == GLFW_RELEASE) {
            keyHandled[msg->keyCode] = false;
        }
    }

    /********************************************************************
    * OnMouseClick: Handles the MessageMouseClick event when a mouse    *
    *               button is clicked. Prints which mouse button was    *
    *               clicked.                                            *
    *               - msg: Pointer to the mouse click message.          *
    ********************************************************************/
    void TestSystem::OnMouseClick(MessageMouseClick* msg) {
        std::cout << "TestSystem received mouse click message: Button " << msg->button << std::endl;
		ImguiManager::Log("TestSystem received mouse click message: Button " + std::to_string(msg->button), "TestSystem");
    }

    /********************************************************************
    * OnMouseMoved: Handles the MessageMouseMove event when the mouse   *
    *               is moved. Prints the current mouse position (x, y). *
    *               - msg: Pointer to the mouse movement message.       *
    ********************************************************************/
    void TestSystem::OnMouseMoved(MessageMouseMove* msg) {
        std::cout << "Mouse moved to position: (" << msg->x << ", " << msg->y << ")" << std::endl;
		ImguiManager::Log("Mouse moved to position: (" + std::to_string(msg->x) + ", " + std::to_string(msg->y) + ")", "TestSystem");
    }

}
