///////////////////////////////////////////////////////////////////////////////
//  InputHandler.cpp
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "InputHandler.h"
#include <iostream>
#include <EngineState.h>
#include <imgui.h>
#include <Camera.h>
#include "Graphics.h"

namespace Framework {

    InputHandler* InputHandler::instance = nullptr;
    static bool tabPressed = false;

    // [FIX] Static Initialize Implementation
    void InputHandler::Initialize(GLFWwindow* window) {
        if (!instance) {
            instance = new InputHandler(window);
            std::cout << " System: Input Handler Initialized" << std::endl;
        }
    }

    // [FIX] Static Shutdown Implementation
    void InputHandler::Shutdown() {
        if (instance) {
            delete instance;
            instance = nullptr;
            std::cout << " System: Input Handler Singleton Deleted" << std::endl;
        }
    }

    InputHandler::InputHandler(GLFWwindow* window)
        : window(window), mouseX(0), mouseY(0) {
    }

    // [FIX] Destructor Implementation
    InputHandler::~InputHandler() {
        keyStates.clear();
        mouseButtonStates.clear();
        messageSystem = nullptr;
        mainCamera = nullptr;
        std::cout << "System: Input Handler Shutdown" << std::endl;
    }

    void InputHandler::SetMessageSystem(MessageSystem* msgSystem) {
        messageSystem = msgSystem;
    }

    void InputHandler::Initialize() {
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, KeyCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, CursorPositionCallback);
    }

    void InputHandler::SetCameraRotationEnabled(bool enabled) {
        m_cameraRotationEnabled = enabled;
        if (enabled) {
            m_firstMouse = true;
        }
    }

    void InputHandler::Update(float deltaTime) {
        (void)deltaTime;

        for (auto& [key, state] : keyStates) {
            if (state == KEY_PRESSED) state = KEY_HELD;
        }
        for (auto& [button, state] : mouseButtonStates) {
            if (state == KEY_PRESSED) state = KEY_HELD;
        }

        glfwPollEvents();

        // TAB: Toggle Editor vs Game Mode
        if (IsKeyPressed(GLFW_KEY_TAB)) {
            if (!tabPressed) {
                tabPressed = true;
                isUIFocused = !isUIFocused;

                if (isUIFocused) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                else {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    m_firstMouse = true;
                }
            }
        }
        else {
            tabPressed = false;
        }

        // ESC: Toggle pause/resume
        if (IsKeyPressed(GLFW_KEY_ESCAPE))
        {
            // Flip the paused state
            engineState.SetPaused(!engineState.IsPaused());

            std::cout << "ESC PRESSED" << std::endl;
        }

        // ImGui Input Block
        ImGuiIO& io = ImGui::GetIO();
        if (!isUIFocused) {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
        }
        else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            io.ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;
        }

        // Editor Mode: Right Click to Look
        if (isUIFocused) {
            if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                m_firstMouse = true;
            }
            if (IsMouseButtonReleased(GLFW_MOUSE_BUTTON_RIGHT)) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
    }

    // [FIX] CursorPositionCallback Implementation
  // In InputHandler.cpp

    void InputHandler::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
        InputHandler* self = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
        if (!self) return;

        self->mouseX = xpos;
        self->mouseY = ypos;

        bool isRightClickHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        bool canRotate = (!self->isUIFocused) || (self->isUIFocused && isRightClickHeld);

        if (!self->m_cameraRotationEnabled) {
            canRotate = false;
        }

        Camera* targetCam = nullptr;

        if (self->mainGraphics) {
            int camIndex = 0; // Default to Game Camera

            if (self->isUIFocused) {
                // [FIX] In Editor Mode, rotate the camera belonging to the Viewport we are hovering
                // This allows you to rotate Main Viewport (0) OR Secondary Viewport (1)
                camIndex = self->mainGraphics->GetMouseTargetView();
            }
            else {
                // In Game Mode, always force Game Camera
                camIndex = 0;
            }

            targetCam = self->mainGraphics->GetCameraByIndex(camIndex);
        }
        else {
            // Fallback if SetGraphics wasn't called
            targetCam = self->mainCamera;
        }

        if (canRotate && targetCam)
        {
            if (self->m_firstMouse) {
                self->m_lastX = xpos;
                self->m_lastY = ypos;
                self->m_firstMouse = false;
            }

            float xoffset = static_cast<float>(xpos - self->m_lastX);
            float yoffset = static_cast<float>(self->m_lastY - ypos);

            self->m_lastX = xpos;
            self->m_lastY = ypos;

            targetCam->processMouseMovement(xoffset, yoffset);
        }
        else {
            self->m_firstMouse = true;
        }
    }
    void InputHandler::SetCamera(Camera* camera) {
        this->mainCamera = camera;
    }

    bool InputHandler::IsKeyPressed(int key) {
        auto it = keyStates.find(key);
        if (it != keyStates.end()) return it->second == KEY_PRESSED;
        return false;
    }

    bool InputHandler::IsKeyHeld(int key) {
        return keyStates[key] == KEY_PRESSED || keyStates[key] == KEY_HELD;
    }

    bool InputHandler::IsKeyReleased(int key) {
        return keyStates[key] == KEY_RELEASED;
    }

    bool InputHandler::IsMouseButtonPressed(int button) {
        if (mouseButtonStates[button] == KEY_PRESSED) {
            mouseButtonStates[button] = KEY_HELD;
            return true;
        }
        return false;
    }

    bool InputHandler::IsMouseButtonHeld(int button) {
        return mouseButtonStates[button] == KEY_HELD || mouseButtonStates[button] == KEY_PRESSED;
    }

    bool InputHandler::IsMouseButtonReleased(int button) {
        return mouseButtonStates[button] == KEY_RELEASED;
    }

    // [FIX] GetMouseX/Y Implementations
    double InputHandler::GetMouseX() const {
        return mouseX;
    }

    double InputHandler::GetMouseY() const {
        return mouseY;
    }

    void InputHandler::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)mods; (void)scancode;
        InputHandler* inputSystem = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
        if (inputSystem) {
            KeyState currentKeyState{};
            if (action == GLFW_PRESS) currentKeyState = KEY_PRESSED;
            else if (action == GLFW_RELEASE) currentKeyState = KEY_RELEASED;
            else if (action == GLFW_REPEAT) currentKeyState = KEY_HELD;
            inputSystem->keyStates[key] = currentKeyState;
        }
    }

    void InputHandler::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        (void)mods;
        InputHandler* inputSystem = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
        if (inputSystem) {
            KeyState currentMouseState{};
            if (action == GLFW_PRESS) currentMouseState = KEY_PRESSED;
            else if (action == GLFW_RELEASE) currentMouseState = KEY_RELEASED;
            inputSystem->mouseButtonStates[button] = currentMouseState;
        }
    }
}