///////////////////////////////////////////////////////////////////////////////
///
///	@file GraphicsWindows.cpp
/// 
/// @brief Creates graphics windows for usage in viewport settings
///	
///	@Authors: Victor Lee / Joshua Sim 
///	Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "GraphicsWindows.h"
#include "Core.h"


namespace Framework {
    
    float GraphicsWindows::FPS{};
    InputHandler* GraphicsWindows::InputHandler{};
    /**
     * @brief Constructor for GraphicsWindows.
     *
     * Initializes a window using GLFW with the given configuration settings and links it to the core engine.
     *
     * @param config The configuration settings for the window (width, height, and title).
     * @param CorePointer A pointer to the core engine, used to signal when the window should close.
     */
    GraphicsWindows::GraphicsWindows(const Window::WindowConfig& config, CoreEngine* CorePointer)
        : screenWidth(config.x), screenHeight(config.y), windowTitle(config.programName), 
          window(nullptr), isInitialized(false), CorePointer(CorePointer) {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return;
        }

        window = glfwCreateWindow(screenWidth, screenHeight, windowTitle.c_str(), nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(window);  // Make the OpenGL context current
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
        glViewport(0, 0, screenWidth, screenHeight);

        isInitialized = true;
    }

    /**
    * @brief Destructor for GraphicsWindows.
    *
    * Cleans up resources by destroying the GLFW window and terminating GLFW.
    */
    GraphicsWindows::~GraphicsWindows() {
        if (window) {
            glfwDestroyWindow(window);  // Clean up the GLFW window
            std::cout << "Window destroyed" << std::endl;
        }

        glfwTerminate();  // Terminate GLFW
    }

    /**
   * @brief Initializes the GraphicsWindows system.
   *
   * Currently, this function does not perform additional setup since initialization happens in the constructor.
   */
    void GraphicsWindows::Initialize() {
       
        InputHandler = InputHandler::GetInstance();
    }

    /**
      * @brief Updates the GraphicsWindows system.
      *
      * Renders content to the window, calculates FPS, and checks for window close events.
      *
      * @param deltaTime The time difference (in seconds) since the last frame update.
      */
    void GraphicsWindows::Update(float deltaTime) {

        // Input Example
        if(InputHandler->IsKeyPressed(GLFW_MOUSE_BUTTON_LEFT))
            std::cout << "CLICK";

        if (!isInitialized) {
            return;
        }
        // Check if the window should close
        if (glfwWindowShouldClose(window)) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);  // Flag to close the window
            CorePointer->EndGameLoop();
        }

        // FPS update
        static double accumulatedTime = 0.0;
        static int frameCount = 0;

        // Increment the frame count and accumulate deltaTime
        frameCount++;
        accumulatedTime += deltaTime;  // Use deltaTime (dt) instead of glfwGetTime()

        // Update FPS every second (when accumulated time reaches 1 second)
        if (accumulatedTime >= 1.0) 
        {
            // Calculate FPS as frames per second
            GraphicsWindows::FPS = (float)(frameCount / accumulatedTime);
        
            //Update the window title with FPS
            std::string title = "Unnamed Engine | M1 Build | FPS | " + std::to_string(GraphicsWindows::FPS); 
            glfwSetWindowTitle(window, title.c_str());

            // Reset the accumulated time and frame count
            accumulatedTime = 0.0;
            frameCount = 0;
        }

        glfwSwapBuffers(window);

    }

    /**
     * @brief Checks if the window is still open.
     *
     * @return True if the window is open, false otherwise.
     */
    bool GraphicsWindows::IsWindowOpen() const {
        return !glfwWindowShouldClose(window);
    }

    /**
    * @brief Returns the GLFW window pointer.
    *
    * @return Pointer to the GLFW window.
    */
    GLFWwindow* GraphicsWindows::GetWindow(){
        return window;
    }

    void GraphicsWindows::framebuffer_size_callback(GLFWwindow* window, int width, int height) 
    {
        (void)window;
        // Update the viewport with the new window size
        glViewport(0, 0, width, height);
    }

}
