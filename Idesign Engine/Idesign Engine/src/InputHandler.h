///////////////////////////////////////////////////////////////////////////////
//
//	\file InputHandler.h
//	
//  * This system uses GLFW callbacks to capture key presses, mouse clicks, and 
//  * cursor movement.It provides functions to check the state of input events,
//  * such as whether a key or mouse button is pressed, held, or released.
//	
//	Authors: Joshua Sim Yue Chen (joshuayuechen.sim@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "System.h"
#include <GLFW/glfw3.h>
#include <unordered_map>
#include "MessageSystem.h"  // Include the MessageSystem header
#include "Audio.h"
#include "Camera.h"

namespace Framework {

    class InputHandler : public ISystem {
    public:
        // Delete copy constructor and assignment operator to prevent copying
        InputHandler(const InputHandler&) = delete;
        InputHandler& operator=(const InputHandler&) = delete;
        ~InputHandler();

        // Static method to access the singleton instance of InputHandler
        static InputHandler* GetInstance() {
            if (!instance) {
               // std::cerr << "Error: InputHandler instance is not initialized!" << std::endl;
                return nullptr;
            }
            //std::cout << "Instance found" << std::endl;
            return instance;
        }

        // Initialize the singleton instance with window and AudioManager (only called once)
        static void Initialize(GLFWwindow* window) {
            if (!instance) {
                instance = new InputHandler(window);
                std::cout << " System: Input Handler Initialized" << std::endl;
            }
        }

        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override { return "InputHandler"; }

        // Inject the MessageSystem after construction
        void SetMessageSystem(MessageSystem* msgSystem);

        // Key and Mouse input states
        bool IsKeyPressed(int key);
        bool IsKeyHeld(int key);
        bool IsKeyReleased(int key);
        bool IsMouseButtonPressed(int button);
        bool IsMouseButtonHeld(int button);
        bool IsMouseButtonReleased(int button);
        double GetMouseX() const;
        double GetMouseY() const;

        bool isUIFocused = true;

        void SetCamera(Camera* camera);
    private:
        // Enum for tracking key and mouse states
        enum KeyState {
            KEY_RELEASED = 0,
            KEY_PRESSED = 1,
            KEY_HELD = 2
        };

        // Reference to MessageSystem
        MessageSystem* messageSystem = nullptr;

        GLFWwindow* window;  // Reference to the GLFW window
        std::unordered_map<int, KeyState> keyStates;  // Stores key states
        std::unordered_map<int, KeyState> mouseButtonStates;  // Stores mouse button states
        double mouseX, mouseY;  // Stores current mouse position

        // Singleton instance
        static InputHandler* instance;

        // Private constructor for singleton pattern
        InputHandler(GLFWwindow* window);

        // Callback functions for input handling
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
        Camera* mainCamera = nullptr;
    };
}