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

#include "ShaderProgram.h"
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "Lighting.h"
#include "MeshLoader.h"
#include "TextureLoader.h"

extern Framework::FontSystem fontSystem;
extern Framework::Coordinator ecsInterface;


namespace Framework {
    //ShaderProgram terrainShader;
    //ShaderProgram meshShader;
    //std::vector<PointLightCPU> pointLights;
    //std::vector<SpotLightCPU> spotLights;
    //DirectionalLightCPU sun; // defaults are fine
    bool Graphics::mouseCaptured = true;

    static const unsigned int WIDTH = 1280;
    static const unsigned int HEIGHT = 720;

    static float lastX = WIDTH * 0.5f;
    static float lastY = HEIGHT * 0.5f;
    static bool firstMouse = true;
    static bool mouseCaptured = true;
    bool groundMode = true; // toggle with a key (e.g., F1)

    // Constructor
    Graphics::Graphics()
    {

    }

    // Destructor
    Graphics::~Graphics()
    {

    }

    // Initialize the system
    void Graphics::Initialize()
    {
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "ECS 3D Renderer", nullptr, nullptr);
        if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return; }
        glfwMakeContextCurrent(window);

        //glewExperimental = GL_TRUE; //optional to include can remove
        if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return; }

        // always enable for depth test framebuffering
        glEnable(GL_DEPTH_TEST);

        // set callbacks
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        // IMPORTANT: always store Graphics* as user pointer, not Camera*
        glfwSetWindowUserPointer(window, &camera);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // asset load
        MeshLoader::loadMesh("Assets/Objects/player.obj", mesh);
        tex = TextureLoader::loadTexture("Assets/PNG/test.png");
        cube = Mesh::CreateUnitCubeData();

        // Initialisers for OpenGL (IMPORTANT : always call these after every initialiser values)
        // If want to change any values, please change in header or reconstruct after calling
        terrainShader.initGL();
        meshShader.initGL();
        terrain.initGL();
        cube.initGL();

        // lights (use member containers)
        sun = DirectionalLightCPU{};
        pointLights.push_back({ {5,3,0},  {1,0.2f,0.2f}, 1.5f, 12.0f, true });
        pointLights.push_back({ {-3,2,-5},{0.2f,0.2f,1}, 1.2f, 14.0f, true });
        spotLights.push_back({ {0,3,0}, {0,-1,0}, {1,1,1}, 2.0f, 18.0f,
                               glm::radians(10.f), glm::radians(15.f), true });

        lastTime = (float)glfwGetTime();
    }

    // Update the system
    void Graphics::Update(float deltaTime)
    {
        while (!glfwWindowShouldClose(window)) {
            float now = (float)glfwGetTime();
            float dt = now - lastTime; lastTime = now;
            process_input(dt);

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

            // extra cubes (untextured or textured as you like)
            for (int i = 0; i < 5; ++i) {
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3((i - 2) * 1.7f, 0.0f, 0.0f));
                meshShader.setMat4("uModel", model);
                cube.draw();
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    // Get the name of the system
    std::string Graphics::GetName()
    {
        return "Graphics";
    }

    void Graphics::framebuffer_size_callback(GLFWwindow* window, int w, int h)
    {
        glViewport(0, 0, w, h);
    }

    // DO NOT CHANGE
    void Graphics::mouse_callback(GLFWwindow* window, double xpos, double ypos)
    {
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
    }

}