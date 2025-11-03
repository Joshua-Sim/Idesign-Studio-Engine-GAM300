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
///	Copyright 2025, Digipen Institute of Technology
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
#include "Coordinator.h"
#include "ComponentList.h"
#include "FontSystem.h"
#include <rttr/type>
#include <rttr/registration>


extern Framework::FontSystem fontSystem;
extern Framework::Coordinator ecsInterface;

namespace Framework {
    bool Graphics::mouseCaptured = true;
    static bool firstMouse = true;
    static bool mouseCaptured = true;
    bool groundMode = true; // toggle with a key (e.g., F1)

    const auto& assets = AssetManager::Instance().GetAssets();

    // Constructor
    Graphics::Graphics()
    {
        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>(AssetManager::Instance().FindGuid<WindowConfig>());
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(config->x, config->y, config->programName.c_str(), nullptr, nullptr);
        if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(0); //disable V Sync

        //glewExperimental = GL_TRUE; //optional to include can remove
        if (glewInit() != GLEW_OK) { std::cerr << "GLEW init failed\n"; return; }
    }

    // Destructor
    Graphics::~Graphics()
    {
        // Delete HDR framebuffer resources
        if (hdrFBO) glDeleteFramebuffers(1, &hdrFBO);
        if (colorBuffer) glDeleteTextures(1, &colorBuffer);
        if (rboDepth) glDeleteRenderbuffers(1, &rboDepth);

        // Delete bloom ping-pong framebuffers & textures
        glDeleteFramebuffers(2, pingpongFBO);
        glDeleteTextures(2, pingpongColorbuffers);

        // Delete screen quad VAO/VBO
        if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
        if (quadVBO) glDeleteBuffers(1, &quadVBO);

        // Cleanup geometry meshes (if they own VAOs/VBOs)
        cube.cleanupGL();
        plane.cleanupGL();
        cone.cleanupGL();
        cylinder.cleanupGL();

        // Cleanup shaders (if your ShaderProgram has a cleanup function)
        terrainShader.destroy();
        meshShader.destroy();
        blurShader.destroy();
        finalShader.destroy();

        // Finally, destroy GLFW window + terminate GLFW
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }

    // Initialize the system
    void Graphics::Initialize()
    {
        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>(AssetManager::Instance().FindGuid<WindowConfig>());

        // always enable for depth test framebuffering
        glEnable(GL_DEPTH_TEST);

        //// set callbacks
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        //// IMPORTANT: always store Graphics* as user pointer, not Camera*
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Register Graphics related Component
        ecsInterface.RegisterComponent<TransformComponent>();
        ecsInterface.RegisterComponent<HierarchyComponent>(); // Register in graphics as its default generic component allocated to all entities.
        ecsInterface.RegisterComponent<MeshProperties>();
        ecsInterface.RegisterComponent<TextureProperties>();
        ecsInterface.RegisterComponent<TextProperties>();

        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<TransformComponent>()); //Set mEntities to be populated with MeshComponent for now

        ecsInterface.SetSystemSignature<Graphics>(signature);
        std::cout << "Signature for Graphics system is: " << signature << std::endl;
        //  ---- ECS INITIALIZATION END --- 


        // ---------------- Create one ECS object (Deserialized next time) -------------------- 
        Entity newEntity = ecsInterface.CreateEntity();

        //Add transform
        TransformComponent transformComponent;
        transformComponent.position = glm::vec3(0.0, 2.0, 0.0); // Instantiate location
        transformComponent.scale = glm::vec3(0.0005, 0.0005, 0.0005); // Size of my obj is big, change for test

        ecsInterface.AddComponent<TransformComponent>(newEntity, transformComponent);

        ecsInterface.AddComponent<HierarchyComponent>(newEntity, HierarchyComponent{});

        // Add and load mesh , temporarily hard-code component data 
        MeshProperties meshComponent;
        meshComponent.path = "Assets/Objects/plane.obj";

        // Load mesh from its own path, again serialized for each obj in deserialization after.
        MeshLoader::loadMesh(meshComponent.path, meshComponent);
        ecsInterface.AddComponent<MeshProperties>(newEntity, meshComponent);

        // Add and load a texture component
        Framework::TextureProperties textureComponent;
        textureComponent.path = "Assets/PNG/test.png";
        TextureLoader::loadTexture(textureComponent, true);



        ecsInterface.AddComponent<Framework::TextureProperties>(newEntity, textureComponent);
        // ----------------- End object creation ------------------------------

        // asset load / loading shape units
        // Testing (Edwin)
        /*for (const auto& [handle, entry] : assets)
        {
            auto* texProps = static_cast<TextureProperties*>(entry.pObject.get());

            texProps->textureID = TextureLoader::loadTexture(*texProps, true);

            tex = texProps->textureID;
        }*/
        AssetManager::Instance().LoadBinaryArray<TextureProperties>("src/Data/TextureAsset.bin", g_TexturePropertiesProps,
            [](TextureProperties& props)
            {
                TextureLoader::loadTexture(props, true);
            });

        AssetManager::Instance().LoadBinaryArray<MeshProperties>("src/Data/MeshAsset.bin", g_MeshProps,
            [](MeshProperties& props)
            {
                MeshLoader::loadMesh(props.path, props);
            });
        //AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, "src/Data/MeshAsset.json", "mesh");
        //AssetManager::Instance().PrintAssetsOfType(TypeName<TextureProperties>::value);
        //AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, "src/Data/TextureAsset.json", "textures");
        cube = Mesh::CreateUnitCubeData();
        plane = Mesh::CreateUnitSphereData();
        cone = Mesh::CreateUnitConeData();
        cylinder = Mesh::CreateUnitCylinderData();
        triangle = Mesh::CreateUnitTriangleData();
        // Initialise 1 object into ECS (Temporary, liase and move de-serialize with loadmesh function handled at asset-handler)


        // Initialisers for OpenGL (IMPORTANT : always call these after every initialiser values)
        // If want to change any values, please change in header or reconstruct after calling
        terrainShader.initGL();
        meshShader.initGL();
        blurShader.initGL();
        finalShader.initGL();
        terrain.initGL();
        debugShader.initGL();
        //initialise geometry objects
        cube.initGL();
        plane.initGL();
        cone.initGL();
        //cylinder.initGL();
        triangle.initGL();
        //FONT SYSTEM

        fontSystem.Initialize();
        fontSystem.LoadFont("Assets/Font/arial.ttf", 40, "Arial");
        fontSystem.SetActiveFont("Arial");
        m_debugMeshes[DebugShapeType::Box] = &cube;
        m_debugMeshes[DebugShapeType::Sphere] = &plane; // Remember 'plane' is your sphere mesh
        m_debugMeshes[DebugShapeType::Triangle] = &triangle;

        // lights (use member containers)
        sun = DirectionalLightCPU{};
        pointLights.push_back({ {5,3,0},  {1,0.2f,0.2f}, 1.5f, 12.0f, true });
        pointLights.push_back({ {-3,2,-5},{0.2f,0.2f,1}, 1.2f, 14.0f, true });
        spotLights.push_back({ {0,3,0}, {0,-1,0}, {1,1,1}, 2.0f, 18.0f,
                               glm::radians(10.f), glm::radians(15.f), true });

        //jay double buffer pick fbo
        auto makePickFBO = [&](int w, int h, int idx) {
            glGenFramebuffers(1, &pickFBO[idx]);
            glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[idx]);

            glGenTextures(1, &pickColor[idx]);
            glBindTexture(GL_TEXTURE_2D, pickColor[idx]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickColor[idx], 0);

            glGenRenderbuffers(1, &pickDepth[idx]);
            glBindRenderbuffer(GL_RENDERBUFFER, pickDepth[idx]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pickDepth[idx]);

            GLenum drawBuf = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &drawBuf);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "ERROR: pick FBO " << idx << " incomplete!\n";

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            };

        int w = 1280, h = 720;

        pickShader.initGL();
        makePickFBO(w, h, 0);
        makePickFBO(w, h, 1);

        // --- HDR FRAMEBUFFER SETUP ---
        glGenFramebuffers(1, &hdrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glGenTextures(1, &colorBuffer);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, config->x, config->y, 0, GL_RGBA, GL_FLOAT, NULL);
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

        m_frameCount = 0;
        m_fpsLastTime = glfwGetTime(); // Start the timer
    }

    void Graphics::SetImguiManager(ImguiManager* manager)
    {
        m_imguiManager = manager;
    }

    // Update the system
    void Graphics::Update(float deltaTime)
    {
        (void)deltaTime;

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

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        if (w == 0 || h == 0) {
            return; // Don't render anything if the window is minimized
        }
        float aspect = (h == 0) ? 1.f : float(w) / float(h);
        glViewport(0, 0, w, h);
        glClearColor(0.07f, 0.09f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Camera& cam = gameCamera;
        glm::mat4 proj = cam.getProjectionMatrix((float)w, (float)h);
        glm::mat4 view = cam.getViewMatrix();


        // stick to ground
        if (groundMode)
        {
            float groundY = terrain.getHeightAt(cam.position.x, cam.position.z);
            cam.position.y = groundY + 1.8f;
        }

        // --- Terrain (no texture)
        terrainShader.use();
        terrainShader.setMat4("uProj", proj);
        terrainShader.setMat4("uView", view);
        terrainShader.setMat4("uModel", glm::mat4(1.f));
        terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f }); //terrain color
        LightComponent::upload(terrainShader, sun, hasSun, pointLights, spotLights);
        terrain.draw();

        // --- Mesh (textured)
        meshShader.use();
        meshShader.setMat4("uProj", proj);
        meshShader.setMat4("uView", view);

        // ---- ECS draw loop ----
    // JAY (374 TO 415)
    // --- highlight selected entity (picked last frame) ---
        int sel = lastPickedId; // cache once per frame
        const glm::vec3 kNormalAlbedo(1.0f, 1.0f, 1.0f);
        const glm::vec3 kHighlightAlbedo(1.0f, 0.85f, 0.3f);
        //JAY
        // ---- ECS draw loop ----
        for (auto const& entityId : mEntities) {
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);

            glm::mat4 model(1.0f);
            model = glm::translate(model, tr.position);
            model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
            model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
            model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
            model = glm::scale(model, tr.scale);

            // Text (optional)
            if (ecsInterface.HasComponent<TextProperties>(entityId)) {
                auto& txt = ecsInterface.GetComponent<TextProperties>(entityId);
                fontSystem.RenderText3D(txt.text, model, view, proj, txt.scale, txt.color);
            }

            // Mesh (optional)
            if (ecsInterface.HasComponent<MeshProperties>(entityId)) {
                auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);

                if (mesh.debugShape == DebugShapeType::None) {
                    meshShader.use();
                    meshShader.setMat4("uProj", proj);
                    meshShader.setMat4("uView", view);
                    meshShader.setMat4("uModel", model);

                    const bool isSelected = (int)entityId == sel;
                    meshShader.setVec3("uAlbedo", isSelected ? kHighlightAlbedo : kNormalAlbedo);
                    LightComponent::upload(meshShader, sun, hasSun, pointLights, spotLights);

                    glActiveTexture(GL_TEXTURE0);
                    meshShader.setInt("uTexture", 0);
                    if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                        glBindTexture(GL_TEXTURE_2D, tex.textureID);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }

                    glBindVertexArray(mesh.vao);
                    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                }
                else {
                    auto it = m_debugMeshes.find(mesh.debugShape);
                    if (it != m_debugMeshes.end()) {
                        DrawDebugMesh(it->second, mesh.debugcolor, model, proj, view);
                    }
                }
            }


        }

        // drawing of 5 objects (primitives)
        for (int i = 0; i < 5; ++i)
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3((i - 2) * 1.7f, 0.0f, 0.0f));
            meshShader.setMat4("uModel", model);
            cylinder.draw(); //change line to whichever you want to test : 1. cube 2. cylinder 3. plane 4.cone follow by .draw();
        }
        // object pick
        //jay
        //render color-IDs into offscreen (double-buffered)
        glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[pickWrite]);

        // IMPORTANT: viewport must match the pick FBO size
        // If you created pick FBOs with (config->x, config->y), use those.
        // If you created them at 1280x720, set (1280,720) here OR recreate the FBOs to match (w,h).
        glViewport(0, 0, w, h);

        glEnable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 0); // id=0 => "no hit"
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        pickShader.use();
        pickShader.setMat4("uProj", proj);
        pickShader.setMat4("uView", view);

        // ECS draw loop with encoded ID color (same transforms as scene)
        for (auto const& entityId : mEntities) {

            auto& transformComponent = ecsInterface.GetComponent<TransformComponent>(entityId);

            glm::mat4 model(1.0f);
            model = glm::translate(model, transformComponent.position);
            model = glm::rotate(model, transformComponent.rotation.x, glm::vec3(1, 0, 0));
            model = glm::rotate(model, transformComponent.rotation.y, glm::vec3(0, 1, 0));
            model = glm::rotate(model, transformComponent.rotation.z, glm::vec3(0, 0, 1));
            model = glm::scale(model, transformComponent.scale);
            pickShader.setMat4("uModel", model);

            uint32_t id = static_cast<uint32_t>(entityId) + 1u; // 0 reserved for "no hit"
            glm::vec3 idColor = {
                ((id) & 0xFF) / 255.0f,
                ((id >> 8) & 0xFF) / 255.0f,
                ((id >> 16) & 0xFF) / 255.0f
            };
            pickShader.setVec3("uIdColor", idColor);
            if (ecsInterface.HasComponent<MeshProperties>(entityId))
            {
                auto& meshComponent = ecsInterface.GetComponent<MeshProperties>(entityId);
                glBindVertexArray(meshComponent.vao);
                glDrawElements(GL_TRIANGLES, meshComponent.indexCount, GL_UNSIGNED_INT, 0);
            }

        }

        // (Optional) give your 5 primitives fixed IDs and draw them here too if you want them pickable.
        //jay
        //glBindFramebuffer(GL_FRAMEBUFFER, m_imguiManager->GetGameFramebuffer(/*viewIndex=*/0));

        // rotate buffers so we read the *previous* one this frame
        std::swap(pickWrite, pickRead);
        // READBACK for VIEW 0 (main viewport)
        if (pendingPickUV0.x >= 0.0f && pendingPickUV0.y >= 0.0f) {
            int pw = 1280, ph = 720;
            int px = glm::clamp(int(pendingPickUV0.x * pw), 0, pw - 1);
            int py = glm::clamp(int(pendingPickUV0.y * ph), 0, ph - 1);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[pickRead]);
            unsigned char rgb[3] = { 0,0,0 };
            glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            uint32_t id = (uint32_t)rgb[0] | ((uint32_t)rgb[1] << 8) | ((uint32_t)rgb[2] << 16);
            lastPickedId = (id == 0) ? -1 : int(id) - 1;
            pendingPickUV0 = { -1.0f, -1.0f };
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
        int mainIdx = m_imguiManager->GetMainViewIndex();
        int secIdx = (mainIdx == 0) ? 1 : 0;
        // ====================================================================
        // 3. FINAL PASS: Render to screen with tone mapping and bloom
        // ====================================================================

        glBindFramebuffer(GL_FRAMEBUFFER, m_imguiManager->GetGameFramebuffer(0));
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

        // Now overlay HUD text in 2D
        fontSystem.RenderText2D("Score: 100", 10.f, 10.0f, 1.f,
            glm::vec3(1.0f, 1.0f, 0.5f), (float)w, (float)h);

        // In your main rendering loop (e.g., in Graphics::Update)

        // 1. Get the camera's view and projection matrices
        glm::mat4 viewMatrix = cam.getViewMatrix();
        glm::mat4 projectionMatrix = cam.getProjectionMatrix((float)w, (float)h);

        // 2. Extract camera's right and up vectors from the view matrix
        glm::vec3 cameraRight_world = { viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0] };
        glm::vec3 cameraUp_world = { viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1] };



        // 4. Create the final model matrix by combining translation and rotation
        glm::mat4 textModel = glm::mat4(1.0f);
        textModel = glm::translate(textModel, glm::vec3(0.0f, 5.0f, 0.0f)); // Position in world space

        // 5. Call the RenderText3D function
        fontSystem.RenderText3D("iDesign Engine by iDesign Studio",
            textModel,
            viewMatrix,
            projectionMatrix,
            0.05f, // Scale
            glm::vec3(1.0f, 1.0f, 0.5f)); // Color

        // --- Handle a pick request coming from ImGui ---
        //jay
        if (pendingPickUV.x >= 0.0f && pendingPickUV.y >= 0.0f) {
            int pw = 1280, ph = 720; // keep these in sync with how you created the pick FBOs
            int px = glm::clamp(int(pendingPickUV.x * pw), 0, pw - 1);
            int py = glm::clamp(int(pendingPickUV.y * ph), 0, ph - 1);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[pickRead]);
            unsigned char rgb[3] = { 0,0,0 };
            glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            uint32_t id = (uint32_t)rgb[0] | ((uint32_t)rgb[1] << 8) | ((uint32_t)rgb[2] << 16);
            lastPickedId = (id == 0) ? -1 : int(id) - 1;

            pendingPickUV = { -1.0f, -1.0f };

            if (lastPickedId >= 0) {
                std::cout << "Picked entity: " << lastPickedId << "\n";
            }
        }
        // ======================= VIEW 1 : SCENE (free-fly) ==========================
        {
            // Scene pass -> HDR
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glEnable(GL_DEPTH_TEST);

            // reuse the same w,h computed earlier
            glViewport(0, 0, w, h);
            glClearColor(0.07f, 0.09f, 0.12f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use the FREE-FLY camera for this pass
            Camera& cam1 = sceneCamera;                 // <--- different camera
            glm::mat4 proj1 = cam1.getProjectionMatrix((float)w, (float)h);
            glm::mat4 view1 = cam1.getViewMatrix();

            // (No groundMode lock for sceneCamera)

            // --- Terrain ---
            terrainShader.use();
            terrainShader.setMat4("uProj", proj1);
            terrainShader.setMat4("uView", view1);
            terrainShader.setMat4("uModel", glm::mat4(1.f));
            terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f });
            LightComponent::upload(terrainShader, sun, hasSun, pointLights, spotLights);
            terrain.draw();

            // --- Mesh (textured) ---
            meshShader.use();
            meshShader.setMat4("uProj", proj1);
            meshShader.setMat4("uView", view1);

            int sel1 = lastPickedId;
            const glm::vec3 kNormalAlbedo(1.0f, 1.0f, 1.0f);
            const glm::vec3 kHighlightAlbedo(1.0f, 0.85f, 0.3f);

            for (auto const& entityId : mEntities) {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);

                glm::mat4 model(1.0f);
                model = glm::translate(model, tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);

                if (ecsInterface.HasComponent<TextProperties>(entityId)) {
                    auto& txt = ecsInterface.GetComponent<TextProperties>(entityId);
                    fontSystem.RenderText3D(txt.text, model, view1, proj1, txt.scale, txt.color);
                }

                if (ecsInterface.HasComponent<MeshProperties>(entityId)) {
                    auto& m = ecsInterface.GetComponent<MeshProperties>(entityId);
                    if (m.debugShape == DebugShapeType::None) {
                        meshShader.use();
                        meshShader.setMat4("uProj", proj1);
                        meshShader.setMat4("uView", view1);
                        meshShader.setMat4("uModel", model);

                        const bool isSelected = (int)entityId == sel1;
                        meshShader.setVec3("uAlbedo", isSelected ? kHighlightAlbedo : kNormalAlbedo);
                        LightComponent::upload(meshShader, sun, hasSun, pointLights, spotLights);

                        glActiveTexture(GL_TEXTURE0);
                        meshShader.setInt("uTexture", 0);
                        if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                            auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                            glBindTexture(GL_TEXTURE_2D, tex.textureID);
                        }
                        else {
                            glBindTexture(GL_TEXTURE_2D, 0);
                        }

                        glBindVertexArray(m.vao);
                        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
                    }
                    else {
                        auto it = m_debugMeshes.find(m.debugShape);
                        if (it != m_debugMeshes.end()) {
                            DrawDebugMesh(it->second, m.debugcolor, model, proj1, view1);
                        }
                    }
                }
            }

            // (Optional) your 5 primitives again for view 1
            for (int i = 0; i < 5; ++i) {
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3((i - 2) * 1.7f, 0.0f, 0.0f));
                meshShader.setMat4("uModel", model);
                cylinder.draw();
            }

            // --- PICK PASS for view 1 (reuses same pick FBOs sequentially) ---
            glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[pickWrite]);
            glViewport(0, 0, w, h);
            glEnable(GL_DEPTH_TEST);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            pickShader.use();
            pickShader.setMat4("uProj", proj1);
            pickShader.setMat4("uView", view1);

            for (auto const& entityId : mEntities) {
                auto& t = ecsInterface.GetComponent<TransformComponent>(entityId);
                glm::mat4 model(1.0f);
                model = glm::translate(model, t.position);
                model = glm::rotate(model, t.rotation.x, glm::vec3(1, 0, 0));
                model = glm::rotate(model, t.rotation.y, glm::vec3(0, 1, 0));
                model = glm::rotate(model, t.rotation.z, glm::vec3(0, 0, 1));
                model = glm::scale(model, t.scale);
                pickShader.setMat4("uModel", model);

                uint32_t id = static_cast<uint32_t>(entityId) + 1u;
                glm::vec3 idColor = {
                    ((id) & 0xFF) / 255.0f,
                    ((id >> 8) & 0xFF) / 255.0f,
                    ((id >> 16) & 0xFF) / 255.0f
                };
                pickShader.setVec3("uIdColor", idColor);

                if (ecsInterface.HasComponent<MeshProperties>(entityId)) {
                    auto& mc = ecsInterface.GetComponent<MeshProperties>(entityId);
                    glBindVertexArray(mc.vao);
                    glDrawElements(GL_TRIANGLES, mc.indexCount, GL_UNSIGNED_INT, 0);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            std::swap(pickWrite, pickRead);

            // --- BLOOM (reuse same code) ---
            glDisable(GL_DEPTH_TEST);
            bool horizontal1 = true, first_iteration1 = true;
            unsigned int amount1 = 10;
            blurShader.use();
            for (unsigned int i = 0; i < amount1; i++) {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal1]);
                blurShader.setBool("horizontal", horizontal1);
                glBindTexture(GL_TEXTURE_2D, first_iteration1 ? colorBuffer : pingpongColorbuffers[!horizontal1]);
                renderQuad();
                horizontal1 = !horizontal1;
                if (first_iteration1) first_iteration1 = false;
            }

            // --- FINAL PASS to SECOND ImGui target ---
            glBindFramebuffer(GL_FRAMEBUFFER, m_imguiManager->GetGameFramebuffer(1));
            glViewport(0, 0, w, h);
            glClear(GL_COLOR_BUFFER_BIT);

            finalShader.use();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, colorBuffer);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal1]);
            finalShader.setInt("sceneTexture", 0);
            finalShader.setInt("bloomTexture", 1);
            finalShader.setFloat("exposure", hdrExposure);
            finalShader.setFloat("bloomStrength", bloomStrength);
            renderQuad();

            // Optional HUD for view 1
            fontSystem.RenderText2D("Editor View", 10.f, 10.f, 1.f, { 1,1,0.5f }, (float)w, (float)h);
            // 3D world-space text (needs depth test and the view1/proj1 matrices)
            glEnable(GL_DEPTH_TEST);
            glm::mat4 textModel1(1.0f);
            textModel1 = glm::translate(textModel1, glm::vec3(0.0f, 5.0f, 0.0f));
            fontSystem.RenderText3D("iDesign Engine by iDesign Studio",
                textModel1, /*model*/
                view1,      /*the secondary view's view matrix*/
                proj1,      /*the secondary view's projection matrix*/
                0.05f,
                glm::vec3(1.0f, 1.0f, 0.5f));
            glDisable(GL_DEPTH_TEST);
            // Optional: handle a second view pick here if you use per-view pending UVs
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        // ===================== END VIEW 1 : SCENE (free-fly) ========================

        //Showing FPS
        double currentTime = glfwGetTime();
        double delta = currentTime - m_fpsLastTime;
        m_frameCount++;

        // If more than 1 second has passed, update the window title
        if (delta >= 1.0)
        {
            double fps = static_cast<double>(m_frameCount) / delta;

            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << fps << " FPS";

            // Use the class's 'window' member variable
            glfwSetWindowTitle(window, ss.str().c_str());

            // Reset for the next second
            m_frameCount = 0;
            m_fpsLastTime = currentTime;
        }
        // --- END: FPS Counter Logic ---



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
        return &cam;
    }

    void Graphics::framebuffer_size_callback(GLFWwindow* window, int w, int h)
    {
        glViewport(0, 0, w, h);
    }
    void Graphics::RequestPickUV(float u, float v) {
        pendingPickUV0 = glm::vec2(u, v);
    }
    void Graphics::RequestPickUVSecondary(float u, float v) {
        pendingPickUV1 = glm::vec2(u, v);
    }
    void Graphics::SetPickViewIndex(int index) {
        pickViewIndex = (index <= 0) ? 0 : 1;
    }
    // DO NOT CHANGE
    void Graphics::mouse_callback(GLFWwindow* window, double xpos, double ypos)
    {
        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>(AssetManager::Instance().FindGuid<WindowConfig>());
        static float lastX = config->x * 0.5f;
        static float lastY = config->y * 0.5f;

        if (!mouseCaptured) return;
        if (firstMouse) {
            lastX = (float)xpos;
            lastY = (float)ypos;
            firstMouse = false;
            return;
        }
        float xoffset = (float)xpos - lastX;
        float yoffset = lastY - (float)ypos; // reversed: y ranges bottom->top
        lastX = (float)xpos;
        lastY = (float)ypos;

        auto* g = static_cast<Graphics*>(glfwGetWindowUserPointer(window));
        if (!g) return;
        if (!g->mouseCaptured) return;

        Camera& cam = (g->m_mouseTargetView == 1) ? g->sceneCamera : g->gameCamera;
        cam.processMouseMovement(xoffset, yoffset);
    }

    // FOR FUTURE REFERENCE
    // Can be pushed to input handler
    void Graphics::process_input(float dt)
    {
        const bool editorActive = (m_mouseTargetView == 1);       // 0 = grounded(game), 1 = editor
        Camera& cam = editorActive ? sceneCamera : gameCamera;    // was: Camera& cam = GetActiveCamera();

        // Switch between Scene <-> Game camera
        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS)
        {
            activeCameraMode = CameraMode::Scene;
            groundMode = false;          // disable ground lock
            std::cout << "Switched to Scene Camera (noclip)\n";
        }
        if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS)
        {
            activeCameraMode = CameraMode::Game;
            groundMode = true;           // enable ground stick
            std::cout << "Switched to Game Camera (grounded)\n";
        }

        // Movement control
        const bool useGround = (!editorActive) && groundMode;

        // Movement control
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Forward, dt, useGround, terrain);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Backward, dt, useGround, terrain);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Left, dt, useGround, terrain);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Right, dt, useGround, terrain);

        //  CHANGED: free-fly vertical only when not grounded
        if (!useGround)
        {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Up, dt, false, terrain);
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Down, dt, false, terrain);
        }

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

    void Graphics::MoveCameraFromLua(const std::string& dir, float dt, bool gm) {
        //if (dir == "W") cam.processKeyboard(Camera::Movement::Forward, dt, gm, terrain);
        //else if (dir == "S") cam.processKeyboard(Camera::Movement::Backward, dt, gm, terrain);
        //else if (dir == "A") cam.processKeyboard(Camera::Movement::Left, dt, gm, terrain);
        //else if (dir == "D") cam.processKeyboard(Camera::Movement::Right, dt, gm, terrain);
    }

    void Graphics::NudgeLightFromLua(const std::string& axis, float amount) {
        //if (pointLights.empty()) return;
        //if (axis == "up")    pointLights[0].position.z -= amount;
        //else if (axis == "down")  pointLights[0].position.z += amount;
        //else if (axis == "left")  pointLights[0].position.x -= amount;
        //else if (axis == "right") pointLights[0].position.x += amount;
    }

    void Graphics::ToggleMainLightFromLua() {
        if (pointLights.empty()) return;
        pointLights[0].enabled = !pointLights[0].enabled;
    }

    void Graphics::SetHDRFromLua(float delta) { hdrExposure = std::max(0.0f, hdrExposure + delta); }
    void Graphics::SetBloomFromLua(float delta) { bloomStrength = std::max(0.0f, bloomStrength + delta); }
    void Graphics::SetGroundModeFromLua(bool gm) { groundMode = gm; }
    bool Graphics::GetGroundModeFromLua() const { return groundMode; }

    void Graphics::SetScriptControlEnabled(bool on) { scriptControl_ = on; }
    bool Graphics::IsScriptControlEnabled() const { return scriptControl_; }
    int  Graphics::GetMouseTargetView() const { return m_mouseTargetView; }

    void Graphics::CaptureMouse(bool on) {
        mouseCaptured = on;
        glfwSetInputMode(window, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
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
    //JAY
    void Graphics::RenderViewToFramebuffer(Camera& viewCam, GLuint imguiTargetFBO,
        int screenW, int screenH, int viewIndex)
    {
        // === 1) Scene pass -> HDR ===
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, screenW, screenH);
        glClearColor(0.07f, 0.09f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = viewCam.getProjectionMatrix((float)screenW, (float)screenH);
        glm::mat4 view = viewCam.getViewMatrix();

        // --- Terrain ---
        terrainShader.use();
        terrainShader.setMat4("uProj", proj);
        terrainShader.setMat4("uView", view);
        terrainShader.setMat4("uModel", glm::mat4(1.f));
        terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f });
        LightComponent::upload(terrainShader, sun, hasSun, pointLights, spotLights);
        terrain.draw();

        // --- ECS meshes (copy your existing mesh draw loop here; use meshShader, etc.) ---
        // ... (your ECS loop that sets uModel, uAlbedo, binds texture, glDrawElements)

        // === PICK PASS for this view (render IDs to the per-view pick FBO) ===
        glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[pickWrite]);   // if you later split per-view, index by viewIndex
        glViewport(0, 0, screenW, screenH);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        pickShader.use();
        pickShader.setMat4("uProj", proj);
        pickShader.setMat4("uView", view);

        // ... (your ECS picking draw loop that encodes uIdColor and draws)

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::swap(pickWrite, pickRead);       // if you split per-view, swap pickWrite[viewIndex], pickRead[viewIndex]

        // === 2) Bloom blur ===
        glDisable(GL_DEPTH_TEST);
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        blurShader.use();
        for (unsigned int i = 0; i < amount; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setBool("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffer : pingpongColorbuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration) first_iteration = false;
        }

        // === 3) Final composite into the target ImGui FBO ===
        glBindFramebuffer(GL_FRAMEBUFFER, imguiTargetFBO);
        glViewport(0, 0, screenW, screenH);
        glClear(GL_COLOR_BUFFER_BIT);

        finalShader.use();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        finalShader.setInt("sceneTexture", 0);
        finalShader.setInt("bloomTexture", 1);
        finalShader.setFloat("exposure", hdrExposure);
        finalShader.setFloat("bloomStrength", bloomStrength);
        renderQuad();

        // === Optional: per-view HUD/text ===
        // fontSystem.RenderText2D("Score: 100", 10.f, 10.f, 1.f, {1,1,0.5f}, (float)screenW, (float)screenH);

        // === Handle a queued pick for THIS view ===
        if (pendingPickUV.x >= 0.0f && pendingPickUV.y >= 0.0f) {
            int px = glm::clamp(int(pendingPickUV.x * screenW), 0, screenW - 1);
            int py = glm::clamp(int(pendingPickUV.y * screenH), 0, screenH - 1);
            unsigned char rgb[3] = { 0,0,0 };

            glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[pickRead]); // per-view if split
            glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            uint32_t id = (uint32_t)rgb[0] | ((uint32_t)rgb[1] << 8) | ((uint32_t)rgb[2] << 16);
            lastPickedId = (id == 0) ? -1 : int(id) - 1;               // per-view if you track separate IDs
            pendingPickUV = { -1.f, -1.f };
        }

        // leave bound to the ImGui target; ImGui will consume the texture later
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
    void Graphics::DrawDebugMesh(Mesh* mesh, const glm::vec3& color, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view) {
        if (!mesh) return;

        debugShader.use();

        // The model matrix is now passed in directly
        debugShader.setMat4("projection", projection);
        debugShader.setMat4("view", view);
        debugShader.setMat4("model", model); // Use the provided model matrix
        debugShader.setVec3("color", color);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        mesh->draw();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}