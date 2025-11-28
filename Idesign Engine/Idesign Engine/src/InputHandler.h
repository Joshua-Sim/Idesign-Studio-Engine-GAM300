///////////////////////////////////////////////////////////////////////////////
//  InputHandler.h
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "System.h"
#include <GLFW/glfw3.h>
#include <unordered_map>
#include "MessageSystem.h"
#include "Camera.h"
#include <iostream>

namespace Framework {

    class InputHandler : public ISystem {
    public:
        // Delete copy/move
        InputHandler(const InputHandler&) = delete;
        InputHandler& operator=(const InputHandler&) = delete;

        // [FIX] Destructor Declaration (Must be here!)
        ~InputHandler();

        // Static Access
        static InputHandler* GetInstance() { return instance; }

        // [FIX] Declarations Only (Definitions moved to .cpp)
        static void Initialize(GLFWwindow* window);
        static void Shutdown();

        // System Overrides
        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override { return "InputHandler"; }

        // Setup
        void SetMessageSystem(MessageSystem* msgSystem);
        void SetCamera(Camera* camera);
        void SetGraphics(class Graphics* gfx) { mainGraphics = gfx; }

        // Input Queries
        bool IsKeyPressed(int key);
        bool IsKeyHeld(int key);
        bool IsKeyReleased(int key);
        bool IsMouseButtonPressed(int button);
        bool IsMouseButtonHeld(int button);
        bool IsMouseButtonReleased(int button);

        // [FIX] Declarations Only
        double GetMouseX() const;
        double GetMouseY() const;

        // State Management
        bool isUIFocused = true;
        void SetCameraRotationEnabled(bool enabled);
        bool IsCameraRotationEnabled() const { return m_cameraRotationEnabled; }

        static InputHandler* instance;

    private:
        enum KeyState { KEY_RELEASED = 0, KEY_PRESSED = 1, KEY_HELD = 2 };

        MessageSystem* messageSystem = nullptr;
        GLFWwindow* window;
        Camera* mainCamera = nullptr;
        class Graphics* mainGraphics = nullptr;

        std::unordered_map<int, KeyState> keyStates;
        std::unordered_map<int, KeyState> mouseButtonStates;

        double mouseX, mouseY;

        bool m_firstMouse = true;
        double m_lastX = 0.0;
        double m_lastY = 0.0;
        bool m_cameraRotationEnabled = true;

        // Private constructor
        InputHandler(GLFWwindow* window);

        // Callbacks
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

        // [FIX] Declaration Only
        static void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    };
}