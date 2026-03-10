///////////////////////////////////////////////////////////////////////////////
//  InputHandler.cpp
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "InputHandler.h"
#include <iostream>
#include <EngineState.h>
#include "stb_image.h"
#include <imgui.h>
#include <Camera.h>
#include "Graphics.h"
#include "Coordinator.h"
#include "DialogueSystem.h"
#include "ButtonSystem.h" // Ensure this is included
#include <RecipeBookManager.h>
#include "GameSystem.h"

extern Framework::RecipeBookManager* recipeBookManager;

extern Framework::Coordinator ecsInterface;
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
        if (m_customCursor) {
            glfwDestroyCursor(m_customCursor);
            m_customCursor = nullptr;
        }
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

        // Load your custom cursor here (path example)
        // NOTE: Keep cursor image small (32x32 or 64x64 is safest)
        LoadCustomCursor("Assets/Textures/mouseCursor3.png", 0, 0);
    }

    void InputHandler::SetCameraRotationEnabled(bool enabled) {
        m_cameraRotationEnabled = enabled;
        if (enabled) {
            m_firstMouse = true;
        }
    }

    void InputHandler::Update(float deltaTime) {
        (void)deltaTime;

        // 1. Update Key States
        for (auto& [key, state] : keyStates) {
            if (state == KEY_PRESSED) state = KEY_HELD;
        }
        for (auto& [button, state] : mouseButtonStates) {
            if (state == KEY_PRESSED) state = KEY_HELD;
        }

        glfwPollEvents();

        // 2. Handle State Toggles
        // TAB: Toggle Editor vs Game Mode
        if (IsKeyPressed(GLFW_KEY_TAB)) {
            if (!tabPressed) {
                tabPressed = true;
                isUIFocused = !isUIFocused;
                // Note: We DO NOT set cursor mode here anymore. We just change the boolean.
            }
        }
        else {
            tabPressed = false;
        }

        // ESC: Toggle pause/resume
        if (IsKeyPressed(GLFW_KEY_ESCAPE))
        {
            g_AudioSystem->PlayAudio("event:/ButtonClick");

            if (engineState.Is(EngineMode::Play))
            {
                engineState.PushState(EngineMode::Paused);
                std::cout << "ESC PRESSED - Hiding How To Play page" << std::endl;
            }
            else 
            {
                engineState.PopState();
                std::cout << "ESC PRESSED - Toggling pause" << std::endl;
            }
        }

		// toggle recipe book
        if (IsKeyPressed(GLFW_KEY_R))
        {
            isUIFocused = !isUIFocused;
            GameSystem::bookOpen = !GameSystem::bookOpen;
        }

		// toggle debug reveal
        if (IsKeyPressed(GLFW_KEY_SLASH))
        {
			GameSystem::ToggleDebugReveal();
        }

        // ESC: Toggle pause/resume
        if (IsKeyPressed(GLFW_KEY_M))
        {
            if (!engineState.Is(EngineMode::MapSelection)) {
                engineState.PushState(EngineMode::MapSelection);
            }
            else {
                engineState.PopState();
            }
        }

        

        // ============================================================
            //DIALOGUE INPUT HANDLING
        // ============================================================
        if (engineState.Is(EngineMode::Dialogue) && !isUIFocused) {
            // Only process dialogue-specific input in game mode
            if (IsKeyPressed(GLFW_KEY_SPACE) || IsKeyPressed(GLFW_KEY_ENTER) || IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                auto dialogueSys = ecsInterface.GetSystem<Framework::DialogueSystem>();
                // [FIX] Check if the camera is mid-flight before allowing "Advance"
              // If the camera is still flying, we ignore this input entirely.
                if (dialogueSys && !dialogueSys->isMidFlight) {
                    dialogueSys->Advance();
                }
                else {
                    std::cout << "[Input] Advance blocked: Camera is still flying!" << std::endl;
                }
            }
        }


        // 3. ImGui Configuration
        ImGuiIO& io = ImGui::GetIO();
        if (!isUIFocused) {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
            // Clear the flag so cursor is hidden properly in game
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        }
        else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            io.ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;

            // CRITICAL: Tell ImGui to stop changing the cursor 
            // so your custom GLFW cursor stays active.
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        }

        // 4. DETERMINE DESIRED CURSOR MODE
        // Default assumption: If we are in the Editor (UI Focused), cursor is visible.
        // If we are in Game, cursor is hidden.
        int desiredMode = isUIFocused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;

        // OVERRIDE 1: Editor Camera Look (Right Click)
        if (isUIFocused) {
            if (IsMouseButtonHeld(GLFW_MOUSE_BUTTON_RIGHT)) 
            {
                desiredMode = GLFW_CURSOR_DISABLED;
                if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) m_firstMouse = true;
            }
        }

        // OVERRIDE 2: Menus / Pause / HowToPlay
        // If ANY of these are true, we MUST show the cursor, regardless of Editor/Game mode.
        if (engineState.IsAnyUIOpen())
        {
            desiredMode = GLFW_CURSOR_NORMAL;
        }

        // 5. APPLY CURSOR MODE
        static int currentMode = -1;
        if (currentMode != desiredMode) {
            glfwSetInputMode(window, GLFW_CURSOR, desiredMode);
            currentMode = desiredMode;

            // Apply custom cursor ONLY when we switch TO visible mode
            if (desiredMode == GLFW_CURSOR_NORMAL) {
                ApplyCustomCursor();
            }

            // Reset first mouse if we just hid the cursor
            if (desiredMode == GLFW_CURSOR_DISABLED) {
                m_firstMouse = true;
            }
        }

        //if (currentMode != desiredMode) {
        //    glfwSetInputMode(window, GLFW_CURSOR, desiredMode);
        //    currentMode = desiredMode;

        //    // [FIX] CRITICAL: Reset camera logic when entering Game Mode
        //    // If we switch to DISABLED mode, treating the next mouse move as a "first move"
        //    // prevents the camera from snapping to the old menu cursor position.
        //    if (desiredMode == GLFW_CURSOR_DISABLED) {
        //        m_firstMouse = true;
        //    }
        //}

        // --- Slider Drag Logic -- 
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        if (width > 0 && height > 0 && engineState.Is(EngineMode::Settings))
        {
            float u = static_cast<float>(mouseX) / static_cast<float>(width);
            float v = static_cast<float>(mouseY) / static_cast<float>(height);
            v = 1.0f - v;

            auto buttonSystem = ecsInterface.GetSystem<ButtonSystem>();
            if (buttonSystem) buttonSystem->HandleSliderDrag(u, v);
        }
    }

    // [FIX] CursorPositionCallback Implementation
    // In InputHandler.cpp

    void InputHandler::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) 
    {
        InputHandler* self = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
        if (!self) return;

        self->mouseX = xpos;
        self->mouseY = ypos;

        // --- [FIX] CHECK GLOBAL ENGINE STATE ---
        // If any menu is open, we must NOT rotate the camera.
        bool isRightClickHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        // --- [FIX] UPDATE ROTATION LOGIC ---
        // 1. Editor Mode (isUIFocused): Rotate only if Right Click is held.
        // 2. Game Mode (!isUIFocused): Rotate only if NO menu is open.
        bool canRotate = (self->isUIFocused && isRightClickHeld) || (!self->isUIFocused && !engineState.IsAnyUIOpen());

        if (!self->m_cameraRotationEnabled) canRotate = false;

        Camera* targetCam = nullptr;

        if (self->mainGraphics) 
        {
            int camIndex = 0;
            if (self->isUIFocused) 
            {
                camIndex = self->mainGraphics->GetMouseTargetView();
            }
            targetCam = self->mainGraphics->GetCameraByIndex(camIndex);
        }
        else targetCam = self->mainCamera;

        if (canRotate && targetCam)
        {
            if (self->m_firstMouse) 
            {
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
            // [IMPORTANT] If we can't rotate (Menu Mode), constantly reset firstMouse.
            // This ensures that when we DO return to the game, the first movement
            // is treated as a reset, not a giant jump.
            // --- THE FIX START ---
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            // Normalize Raw Pixels (0 -> 1920) to UVs (0.0 -> 1.0)
            // Check for divide by zero just in case
            if (width > 0 && height > 0) {
                float uvX = static_cast<float>(xpos) / static_cast<float>(width);
                float uvY = 1.0f - (static_cast<float>(ypos) / static_cast<float>(height));

                recipeBookManager->OnMouseMove(uvX, uvY);
            }
            self->m_firstMouse = true;
        }

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        if (width > 0 && height > 0)
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            // Calculate Normalized Coordinates (0.0 to 1.0)
            float u = static_cast<float>(mx) / static_cast<float>(width);
            float v = static_cast<float>(my) / static_cast<float>(height);

            // [FIX] Invert Y-Axis for OpenGL-style rendering
            // If clicking TOP triggered BOTTOM button, this will flip it.
            v = 1.0f - v;

            auto buttonSystem = ecsInterface.GetSystem<ButtonSystem>();
            if (buttonSystem)
            {
                buttonSystem->UpdateHoverUV(u, v);
            }
        }
    }

    void InputHandler::SetRecipeManager(Framework::RecipeBookManager* manager) {
        m_recipeManager = manager;
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

    void InputHandler::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        (void)mods;
        InputHandler* inputSystem = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));

        if (!inputSystem) return;

        // 1. Update State (Keep this!)
        KeyState currentMouseState{};
        if (action == GLFW_PRESS) currentMouseState = KEY_PRESSED;
        else if (action == GLFW_RELEASE) currentMouseState = KEY_RELEASED;
        inputSystem->mouseButtonStates[button] = currentMouseState;

        // 2. Handle UI Clicks
        if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            if (width > 0 && height > 0)
            {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);

                // Calculate Normalized Coordinates (0.0 to 1.0)
                float u = static_cast<float>(mx) / static_cast<float>(width);
                float v = static_cast<float>(my) / static_cast<float>(height);

                // [FIX] Invert Y-Axis for OpenGL-style rendering
                // If clicking TOP triggered BOTTOM button, this will flip it.
                v = 1.0f - v;

                // Debug Print to confirm
                //std::cout << "[Input] Click at UV: (" << u << ", " << v << ")" << std::endl;

                auto buttonSystem = ecsInterface.GetSystem<ButtonSystem>();
                if (buttonSystem)
                {
                    buttonSystem->HandleClickUV(u, v);
                }
                // InputHandler.cpp MouseButtonCallback
                if (inputSystem->m_recipeManager) {
                    inputSystem->m_recipeManager->OnMouseClick(u, v);
                }
            }
        }
    }

    void InputHandler::SetUIFocused(bool focused) {
        isUIFocused = focused;

        // Optional: Reset tab state if forcing game mode
        if (!focused) {
            // Force immediate cursor update if needed, 
            // though Update() loop will catch it next frame.
        }
    }

    bool Framework::InputHandler::LoadCustomCursor(const char* relativePath, int hotX, int hotY)
    {
        std::filesystem::path root = GetEngineRootPath();
        std::filesystem::path abs = root / relativePath;

        if (!std::filesystem::exists(abs)) {
            std::cout << "[Cursor] File not found: " << abs.string() << "\n";
            return false;
        }

        int w = 0, h = 0, comp = 0;
        stbi_uc* pixels = stbi_load(abs.string().c_str(), &w, &h, &comp, 4);
        if (!pixels) {
            std::cout << "[Cursor] stbi_load failed: " << abs.string()
                << " reason=" << stbi_failure_reason() << "\n";
            return false;
        }

        GLFWimage img{ w, h, pixels };

        if (m_customCursor) {
            glfwDestroyCursor(m_customCursor);
            m_customCursor = nullptr;
        }

        m_customCursor = glfwCreateCursor(&img, hotX, hotY);
        stbi_image_free(pixels);

        if (!m_customCursor) {
            std::cout << "[Cursor] glfwCreateCursor failed\n";
            return false;
        }

        ApplyCustomCursor();
        return true;
    }

    void Framework::InputHandler::ApplyCustomCursor()
    {
        if (m_customCursor) {
            glfwSetCursor(window, m_customCursor);
        }
    }
}