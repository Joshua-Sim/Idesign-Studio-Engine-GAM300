///////////////////////////////////////////////////////////////////////////////
///
///	@file Graphics.cpp
/// 
/// @brief Graphics system rendering pipeline. Includes all the functions for:
///        drawing of meshes, creating basic shapes, performing matrix transformation
///        and input of PNG images.
///	
///	@Authors: Victor Lee
///           Dylan Lau
///           Edwin Leow
///           Joel Teo
///           Joshua Sim
///           Keegan Lim
///	Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
//#include <windows.h>
//#include <commdlg.h>
#include "Graphics.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//#include "stb_image_resize2.h"
//#include <InputHandler.h>
//#include "ComponentList.h"
//#include "FontSystem.h"
//#include "Coordinator.h"
//#include <algorithm>
//
//#include "imgui.h"
//#include "imgui_impl_glfw.h"
//#include "imgui_impl_opengl3.h"
//#include "EngineState.h"
//#include "AssetManager.h"
//#include "TextureAsset.h"
//#include "Debugger.h"

#include <glew.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <iostream>
#include "Core.h"
#include "ShaderProgram.h"
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "Lighting.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "ImguiManager.h"

extern Framework::FontSystem fontSystem;
//extern Framework::Coordinator ecsInterface;


namespace Framework {
    //ShaderProgram terrainShader;
    //ShaderProgram meshShader;
    //std::vector<PointLightCPU> pointLights;
    //std::vector<SpotLightCPU> spotLights;
    //DirectionalLightCPU sun; // defaults are fine
    bool Graphics::mouseCaptured = true;

    //static const unsigned int WIDTH = 1280;
    //static const unsigned int HEIGHT = 720;

    //static float lastX = WIDTH * 0.5f;
    //static float lastY = HEIGHT * 0.5f;
    static bool firstMouse = true;
    static bool mouseCaptured = true;
    bool groundMode = true; // toggle with a key (e.g., F1)

    // Constructor
    Graphics::Graphics()
    {
        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>("WindowConfig");
        
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(config->x, config->y, config->programName.c_str(), nullptr, nullptr);
        if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return; }
        glfwMakeContextCurrent(window);

        //glewExperimental = GL_TRUE; //optional to include can remove
        if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return; }
    }

    // Destructor
    Graphics::~Graphics()
    {

    }

    // Initialize the system
    void Graphics::Initialize()
    {
        // always enable for depth test framebuffering
        glEnable(GL_DEPTH_TEST);

        // set callbacks
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        // IMPORTANT: always store Graphics* as user pointer, not Camera*
        glfwSetWindowUserPointer(window, &camera);
        //glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // asset load / loading shape units
        MeshLoader::loadMesh("Assets/Objects/player.obj", mesh);
        tex = TextureLoader::loadTexture("Assets/PNG/test.png");
        cube = Mesh::CreateUnitCubeData();
        plane = Mesh::CreateUnitSphereData();
        cone = Mesh::CreateUnitConeData();
        cylinder = Mesh::CreateUnitCylinderData();

        // Initialisers for OpenGL (IMPORTANT : always call these after every initialiser values)
        // If want to change any values, please change in header or reconstruct after calling
        //terrainShader.initGL();
        meshShader.initGL();
        blurShader.initGL();
        finalShader.initGL();
        terrain.initGL();

        //initialise shapes
        cube.initGL();
        plane.initGL();
        cone.initGL();
        cylinder.initGL();


        // lights (use member containers)
        sun = DirectionalLightCPU{};
        pointLights.push_back({ {5,3,0},  {1,0.2f,0.2f}, 1.5f, 12.0f, true });
        pointLights.push_back({ {-3,2,-5},{0.2f,0.2f,1}, 1.2f, 14.0f, true });
        spotLights.push_back({ {0,3,0}, {0,-1,0}, {1,1,1}, 2.0f, 18.0f,
                               glm::radians(10.f), glm::radians(15.f), true });


        // --- HDR FRAMEBUFFER SETUP ---
        glGenFramebuffers(1, &hdrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glGenTextures(1, &colorBuffer);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, config->x, config->y,0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
        glGenRenderbuffers(1, &rboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, config->x, config->y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "ERROR::FRAMEBUFFER:: HDR Framebuffer is not complete!\n";

        // --- BLOOM (PINGPONG) FRAMEBUFFER SETUP ---
        glGenFramebuffers(2, pingpongFBO);
        glGenTextures(2, pingpongColorbuffers);
        for (unsigned int i = 0; i < 2; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
            glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, config->x, config->y, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "ERROR::FRAMEBUFFER:: Ping-pong FBO is not complete!\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        lastTime = (float)glfwGetTime();
    }


    void Graphics::SetImguiManager(ImguiManager* manager)
    {
        m_imguiManager = manager;
    }

    // Update the system
    void Graphics::Update(float deltaTime)
    {
        if (glfwWindowShouldClose(window)) {
            CORE->EndGameLoop(); // Mark the game loop as done
            return;
        }
        
            float now = (float)glfwGetTime();
            float dt = now - lastTime; lastTime = now;
            process_input(dt);


            // ====================================================================
           // 1. SCENE PASS: Render the scene to the HDR framebuffer
           // ====================================================================
                glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glEnable(GL_DEPTH_TEST);

            int w, h; glfwGetFramebufferSize(window, &w, &h);
            float aspect = (h == 0) ? 1.f : float(w) / float(h);
            glViewport(0, 0, w, h);
            glClearColor(0.07f, 0.09f, 0.12f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 proj = glm::perspective(glm::radians(camera.zoom), aspect, 0.1f, 500.0f);
            glm::mat4 view = camera.getViewMatrix();

            // stick to ground
            float groundY = terrain.getHeightAt(camera.position.x, camera.position.z);
            camera.position.y = groundY + 1.8f;

            // --- Terrain (no texture)
            terrainShader.use();
            terrainShader.setMat4("uProj", proj);
            terrainShader.setMat4("uView", view);
            terrainShader.setMat4("uModel", glm::mat4(1.f));
            terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f });
            LightComponent::upload(terrainShader, sun, hasSun, pointLights, spotLights);
            terrain.draw();

            // --- Mesh (textured)
            meshShader.use();
            meshShader.setMat4("uProj", proj);
            meshShader.setMat4("uView", view);
            meshShader.setMat4("uModel", glm::mat4(1.f));
            meshShader.setVec3("uAlbedo", { 1,1,1 });
            LightComponent::upload(meshShader, sun, hasSun, pointLights, spotLights);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            meshShader.setInt("uTexture", 0); // correct shader
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);


            // drawing of 5 shapes (testing)
            for (int i = 0; i < 5; ++i) {
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3((i - 2) * 1.7f, 0.0f, 0.0f));
                meshShader.setMat4("uModel", model);
                cylinder.draw(); //change line to whichever you want to test : 1. cube 2. cylinder 3. plane 4.cone follow by .draw();
            }

            // ====================================================================
            // 2. BLOOM PASS: Apply a two-pass Gaussian blur
            // ====================================================================
            glDisable(GL_DEPTH_TEST);
            bool horizontal = true, first_iteration = true;
            unsigned int amount = 10;
            blurShader.use();
            for (unsigned int i = 0; i < amount; i++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
                blurShader.setBool("horizontal", horizontal);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffer : pingpongColorbuffers[!horizontal]);
                renderQuad();
                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }

            // ====================================================================
            // 3. FINAL PASS: Render to screen with tone mapping and bloom
            // ====================================================================
            glBindFramebuffer(GL_FRAMEBUFFER, m_imguiManager->GetGameFramebuffer());
            glViewport(0, 0, w, h);
            glClear(GL_COLOR_BUFFER_BIT);

            finalShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBuffer);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
            finalShader.setInt("sceneTexture", 0);
            finalShader.setInt("bloomTexture", 1);
            finalShader.setFloat("exposure", hdrExposure);
            finalShader.setFloat("bloomStrength", bloomStrength);

           
            renderQuad();

            // Unbind the framebuffer, returning rendering control to the main window
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
 
        
       
    }

    // Get the name of the system
    std::string Graphics::GetName()
    {
        return "Graphics";
    }

    // Get and return windows ptr for external system
    GLFWwindow* Graphics::GetWindow() {
        return window;
    }

    Camera* Graphics::getCamera() {
        return &camera;
    }

    void Graphics::framebuffer_size_callback(GLFWwindow* window, int w, int h)
    {
        glViewport(0, 0, w, h);
    }

    // DO NOT CHANGE
    void Graphics::mouse_callback(GLFWwindow* window, double xpos, double ypos)
    {
        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>("WindowConfig");
        static float lastX = config->x * 0.5f;
        static float lastY = config->y * 0.5f;

        if (!mouseCaptured) return;
        if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
        float xoffset = (float)xpos - lastX;
        float yoffset = lastY - (float)ypos; // reversed: y ranges bottom->top
        lastX = (float)xpos; lastY = (float)ypos;

        Camera* cam = reinterpret_cast<Camera*>(glfwGetWindowUserPointer(window));
        cam->processMouseMovement(xoffset, yoffset);
    }

    // FOR FUTURE REFERENCE
    // Can be pushed to input handler
    void Graphics::process_input(float dt)
    {
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) groundMode = !groundMode;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(Camera::Movement::Forward, dt, groundMode, terrain);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(Camera::Movement::Backward, dt, groundMode, terrain);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(Camera::Movement::Left, dt, groundMode, terrain);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(Camera::Movement::Right, dt, groundMode, terrain);


        //lighting
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            pointLights[0].position.z -= 5.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            pointLights[0].position.z += 5.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            pointLights[0].position.x -= 5.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            pointLights[0].position.x += 5.0f * dt;

        // Toggle light on/off
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        {
            pointLights[0].enabled = !pointLights[0].enabled;
        }

        // HDR and Bloom control
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) hdrExposure += 1.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) if (hdrExposure > 0.0f) hdrExposure -= 1.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) bloomStrength += 1.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) if (bloomStrength > 0.0f) bloomStrength -= 1.0f * dt;
    }

    void Graphics::renderQuad()
    {
        if (quadVAO == 0)
        {
            float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            };
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    //For ImGUI 
    GLuint Graphics::CreateFramebuffer(int width, int height, GLuint& texture, GLuint& rbo_fb)
    {
        // Check for invalid dimensions immediately
        assert(width > 0 && height > 0 && "Framebuffer dimensions cannot be zero!");
        std::cout << "Creating framebuffer with size: " << width << "x" << height << std::endl;


        GLuint framebuffer;

        // Generate and bind framebuffer
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        // Create a color texture to render to
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        // Create a renderbuffer for depth and stencil
        glGenRenderbuffers(1, &rbo_fb);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo_fb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_fb);

        // Check if framebuffer is complete
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "Framebuffer not complete!" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return 0; // Indicate failure
        }

        // Unbind framebuffer to revert to default
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return framebuffer;
    }


}

