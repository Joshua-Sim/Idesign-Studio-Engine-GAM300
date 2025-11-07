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
#include "NavMeshAgentProperties.h"
#include "ImguiManager.h"
#include "Coordinator.h"
#include "ComponentList.h"
#include <NavMeshProperties.h>
#include "FontSystem.h"
#include "PrefabProperties.h"
#include "PrefabComponent.h"
#include "TextureLoaderDDS.h"
#include "PhysicsSystem.h"


extern Framework::FontSystem fontSystem;
extern Framework::Coordinator ecsInterface;

const int   TerrainSize = 128;
const float TerrainScale = 1.0f;
Terrain     terrain{ TerrainSize, TerrainScale };

namespace Framework {
    bool Graphics::mouseCaptured = true;
    static bool firstMouse = true;
    static bool mouseCaptured = true;
    bool Graphics::groundMode = true; // toggle with a key (e.g., F1)

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
        skinnedShader.destroy();

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
        ImguiManager::Log("Initializing Graphics System...", "Graphics");

        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>(AssetManager::Instance().FindGuid<WindowConfig>());

        // always enable for depth test framebuffering
        glEnable(GL_DEPTH_TEST);

        //// set callbacks
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        //// IMPORTANT: always store Graphics* as user pointer, not Camera*
        glfwSetWindowUserPointer(window, this);
        glfwSetCursorPosCallback(window, mouse_callback);

        // Register Graphics related Component
        ImguiManager::Log("Registering graphics components...", "Graphics");
        ecsInterface.RegisterComponent<TransformComponent>();
        ecsInterface.RegisterComponent<HierarchyComponent>(); // Register in graphics as its default generic component allocated to all entities.
        ecsInterface.RegisterComponent<MeshProperties>();
        ecsInterface.RegisterComponent<TextureProperties>();
        ecsInterface.RegisterComponent<TextProperties>();
        ecsInterface.RegisterComponent<NavMeshComponent>();
        ecsInterface.RegisterComponent<NavMeshAgentComponent>(); // Move to navmesh sys after fixing load scene for mesh model
        ecsInterface.RegisterComponent<AudioProperties>();
        ecsInterface.RegisterComponent<PhysicsBodyComponent>();
        ecsInterface.RegisterComponent<MaterialProperties>();
        ecsInterface.RegisterComponent<MaterialMetadata>();
        //test

        ImguiManager::LogSuccess("Graphics components registered", "Graphics");
        ecsInterface.RegisterComponent<PrefabComponent>();

        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<TransformComponent>()); //Set mEntities to be populated with MeshComponent for now

        ecsInterface.SetSystemSignature<Graphics>(signature);

        std::stringstream ss;
        ss << signature;
        std::cout << "Signature for Graphics system is: " << signature << std::endl;
        ImguiManager::Log("Graphics signature set: " + ss.str(), "Graphics");
        //  ---- ECS INITIALIZATION END --- 


        // ---------------- Create one ECS object (Frog Agent) (Deserialized next time) -------------------- 
        Entity newEntity = ecsInterface.CreateEntity(); // FROG ENTITY

        //Add transform
        TransformComponent transformComponent;
        transformComponent.position = glm::vec3(10, 0, -3); // Instantiate location
        transformComponent.scale = glm::vec3(1, 1, 1); // Size of my obj is big, change for test
        transformComponent.rotation = glm::vec3(0, 0, 0);

        ecsInterface.AddComponent<TransformComponent>(newEntity, transformComponent);

        MaterialProperties materialComponent;
        materialComponent.ambient = glm::vec3(0.05f, 0.07f, 0.1f);   // Slightly cool ambient tone
        materialComponent.albedo = glm::vec3(0.8f, 0.6f, 0.3f);      // Warm diffuse color (like wood or bronze)
        materialComponent.specular = glm::vec3(0.9f, 0.9f, 0.95f);   // Strong specular for shiny surface
        materialComponent.shininess = 48.0f;                         // Fairly glossy
        materialComponent.opacity = 0.85f;                           // Slightly transparent

        ecsInterface.AddComponent<MaterialProperties>(newEntity, materialComponent);

        ecsInterface.AddComponent<HierarchyComponent>(newEntity, HierarchyComponent{});

        // Add and load mesh , temporarily hard-code component data 
        MeshProperties meshComponent;
        meshComponent.path = "Assets/Objects/layout2-0.obj";

        // Load mesh from its own path, again serialized for each obj in deserialization after.
        MeshLoader::loadMesh(meshComponent.path, meshComponent);
        ecsInterface.AddComponent<MeshProperties>(newEntity, meshComponent);

        // Add and load a texture component
        Framework::TextureProperties textureComponent;
        textureComponent.path = "Assets/PNG/test.png"; //can put any compressed texture
        TextureLoader::loadTexture(textureComponent, true);

        // Add NavMesh component to mark this as navmesh source
        Framework::NavMeshComponent navMeshComp;
        navMeshComp.isNavMeshSource = true;  // Mark this mesh as navmesh source
        navMeshComp.guid = meshComponent.guid;  // Optional: store reference
        ecsInterface.AddComponent<Framework::NavMeshComponent>(newEntity, navMeshComp);
        ecsInterface.AddComponent<Framework::TextureProperties>(newEntity, textureComponent);
        // ----------------- End object creation ------------------------------


    // ---------------- Create AI Agent Entity -------------------- 
        Entity agentEntity = ecsInterface.CreateEntity();

        // Add transform - position agent ON the navmesh floor
        TransformComponent agentTransform;
        agentTransform.position = glm::vec3(-2, 0.5, 0); // On the floor, slightly offset in X
        agentTransform.scale = glm::vec3(1, 1, 1);    // Small visual size
        agentTransform.rotation = glm::vec3(0, 0, 0);  // Rotated to orient correctly
        ecsInterface.AddComponent<TransformComponent>(agentEntity, agentTransform);
        ecsInterface.AddComponent<MaterialProperties>(agentEntity, materialComponent);
        ecsInterface.AddComponent<HierarchyComponent>(agentEntity, HierarchyComponent{});

        // Add mesh for agent (use cube, sphere, or character model)
        MeshProperties agentMesh;
        agentMesh.path = "Assets/Objects/Frog.obj"; // Or "sphere.obj" or "character.obj"
        MeshLoader::loadMesh(agentMesh.path, agentMesh);
        ecsInterface.AddComponent<MeshProperties>(agentEntity, agentMesh);

        // Set destination here to test

        // Add texture for agent (make it different color so you can see it)
        Framework::TextureProperties agentTexture;
        agentTexture.path = "Assets/PNG/test.png"; // Different texture from floor
        TextureLoader::loadTexture(agentTexture, true);
        ecsInterface.AddComponent<Framework::TextureProperties>(agentEntity, agentTexture);

        // Add NavMeshAgent component - scaled for your scale 3 world
        Framework::NavMeshAgentComponent navAgent;
        navAgent.agentHeight = 2.0f;           // Standard character height in world units
        navAgent.agentRadius = 0.6f;           // Standard radius for pathfinding
        navAgent.moveSpeed = 5.0f;             // Movement speed (units per second)
        navAgent.acceleration = 8.0f;          // How quickly it accelerates
        navAgent.angularSpeed = 120.0f;        // Rotation speed
        navAgent.stoppingDistance = 0.3f;      // Distance to stop before waypoint
        navAgent.targetPosition = glm::vec3(-2.0, 1.0, 1.5); // Destination across the floor
        navAgent.hasPath = false;              // Will be set when path is calculated
        navAgent.isMoving = false;
        ecsInterface.AddComponent<Framework::NavMeshAgentComponent>(agentEntity, navAgent);
        // ----------------- End agent creation ------------------------------
        


        //   //  // ---------------- Create one ANIMATED ECS object (FOR TESTING) --------------------
        Entity animatedEntity = ecsInterface.CreateEntity();

        // 1. Add transform component
        TransformComponent transformComponent1;
        transformComponent1.position = glm::vec3(10,0.0, 0.3); // Instantiate location
        transformComponent1.scale = glm::vec3(0.5, 0.5, 0.5); // Size of my obj is big, change for test
        //transformComponent1.rotation = glm::vec3(90.0f, 0.0f, 0.0f);
        ecsInterface.AddComponent<TransformComponent>(animatedEntity, transformComponent1);
        // 2. Add hierarchy component (standard for your entities)
        ecsInterface.AddComponent<HierarchyComponent>(animatedEntity, HierarchyComponent{});

        // 3. Define the components we need to load into
        MeshProperties animMesh;
        Framework::AnimationProperties animProps;

        // --- !! IMPORTANT: Change this path to your animated model file !! ---
        animMesh.path = "Assets/Objects/knifechoppingwithBONEFIXED4.fbx";

        // 4. Use the new loader that loads mesh, skeleton, and animation data
        MeshLoader::loadMesh(animMesh.path, animMesh, animProps);
        // 5. Add all the new components to the entity
        ecsInterface.AddComponent<MeshProperties>(animatedEntity, animMesh);
        ecsInterface.AddComponent<AnimationProperties>(animatedEntity, animProps);

        // 6. (Optional) Add a texture if your model uses one
        Framework::TextureProperties animTexture;
        // --- !! IMPORTANT: Change this path to your model's texture !! ---
        animTexture.path = "Assets/Textures/music.png";
        TextureLoader::loadTexture(animTexture, true);
        ecsInterface.AddComponent<Framework::TextureProperties>(animatedEntity, animTexture);
        // ----------------- End animated object creation ---------------------


        // asset load / loading shape units
        // Testing (Edwin)
        AssetManager::Instance().LoadBinaryArray<TextureProperties>("Resource/TextureAsset.bin", g_TexturePropertiesProps, [](TextureProperties& props)
            {
                TextureLoader::loadTexture(props, true);
            });

        AssetManager::Instance().LoadBinaryArray<MeshProperties>("Resource/MeshAsset.bin", g_MeshProps, [](MeshProperties& props)
            {
                MeshLoader::loadMesh(props.path, props);
            });

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
        skinnedShader.initGL();
        //initialise geometry objects
        cube.initGL();
        plane.initGL();
        cone.initGL();
        //cylinder.initGL();
        triangle.initGL();
        //FONT SYSTEM

        /*fontSystem.Initialize();
        fontSystem.LoadFont("Assets/Font/arial.ttf", 40, "Arial");
        fontSystem.SetActiveFont("Arial");*/

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

        ImguiManager::LogSuccess("Graphics System initialized successfully", "Graphics");
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

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w == 0 || h == 0) {
            return; // Don't render anything if the window is minimized
        }

        // ====================================================================
        // 1. RENDER VIEW 0: GAME (grounded)
        // ====================================================================
        {
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE); // Added

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


            // ---- START ANIMATION/TRANSPARENCY RENDER LOGIC (VIEW 0) ----

            int sel = lastPickedId; // Use the main picked ID
            const glm::vec3 kHighlightAlbedo(1.0f, 0.85f, 0.3f);

            std::vector<Entity> opaqueEntities;
            std::multimap<float, Entity> transparentEntities;

            for (auto const& entityId : mEntities)
            {
                if (ecsInterface.HasComponent<MeshProperties>(entityId))
                {
                    auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);
                    
                    if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                    {
                        auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);

                        if (material.opacity < 1.0f)
                        {
                            if (ecsInterface.HasComponent<TransformComponent>(entityId))
                            {
                                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                                float distance = glm::distance(tr.position, cam.position);
                                transparentEntities.insert({ distance, entityId });
                            }
                        }
                        else
                        {
                            opaqueEntities.push_back(entityId);
                        }
                    }
                }
            }

            // --- 1. OPAQUE PASS (VIEW 0) ---
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);

            for (auto const& entityId : opaqueEntities)
            {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                glm::mat4 model(1.0f);
                model = glm::translate(model, tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);

                // Text 
                if (ecsInterface.HasComponent<TextProperties>(entityId)) {
                    auto& txt = ecsInterface.GetComponent<TextProperties>(entityId);
                    fontSystem.RenderText3D(txt.text, model, view, proj, txt.scale, txt.color);
                }

                // Mesh 
                if (ecsInterface.HasComponent<MeshProperties>(entityId))
                {
                    auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);

                    if (mesh.debugShape == DebugShapeType::None) {

                        bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(entityId);
                        ShaderProgram& currentshader = isSkinned ? skinnedShader : meshShader;

                        currentshader.use();
                        currentshader.setMat4("uProj", proj);
                        currentshader.setMat4("uView", view);
                        currentshader.setVec3("uViewPos", cam.position);
                        currentshader.setMat4("uModel", model);

                        if (isSkinned) {
                            auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                            for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i)
                            {
                                currentshader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                            }
                        }

                        if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                        {
                            auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);
                            const bool isSelected = (int)entityId == sel;
                            glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : material.albedo; // Use mesh.albedo
                            currentshader.setVec3("uAlbedo", finalAlbedo);

                            glActiveTexture(GL_TEXTURE0);
                            currentshader.setVec3("uMaterial.ambient", material.ambient);
                            currentshader.setVec3("uMaterial.specular", material.specular);
                            currentshader.setFloat("uMaterial.shininess", material.shininess);
                            currentshader.setFloat("uMaterial.opacity", material.opacity);
                            currentshader.setInt("uTexture", 0);
                        }
                        
                        if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                            auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                            glBindTexture(GL_TEXTURE_2D, tex.textureID);
                        }
                        else {
                            glBindTexture(GL_TEXTURE_2D, 0);
                        }

                        LightComponent::upload(currentshader, sun, hasSun, pointLights, spotLights);
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

            // --- 2. TRANSPARENT PASS (VIEW 0) ---
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            for (auto it = transparentEntities.rbegin(); it != transparentEntities.rend(); ++it)
            {
                auto const& entityId = it->second;
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);
                auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);

                glm::mat4 model(1.0f);
                model = glm::translate(model, tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);

                if (mesh.debugShape == DebugShapeType::None)
                {
                    bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(entityId);
                    ShaderProgram& currentShader = isSkinned ? skinnedShader : meshShader;

                    currentShader.use();
                    currentShader.setMat4("uProj", proj);
                    currentShader.setMat4("uView", view);
                    currentShader.setVec3("uViewPos", cam.position);
                    currentShader.setMat4("uModel", model);

                    if (isSkinned)
                    {
                        auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                        for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i)
                        {
                            currentShader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                        }
                    }

                    const bool isSelected = (int)entityId == sel;
                    glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : material.albedo;
                    currentShader.setVec3("uAlbedo", finalAlbedo);
                    currentShader.setVec3("uMaterial.ambient", material.ambient);
                    currentShader.setVec3("uMaterial.specular", material.specular);
                    currentShader.setFloat("uMaterial.shininess", material.shininess);
                    currentShader.setFloat("uMaterial.opacity", material.opacity);

                    glActiveTexture(GL_TEXTURE0);
                    currentShader.setInt("uTexture", 0);
                    if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                        glBindTexture(GL_TEXTURE_2D, tex.textureID);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }

                    LightComponent::upload(currentShader, sun, hasSun, pointLights, spotLights);
                    glBindVertexArray(mesh.vao);
                    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                }
            }

            // --- 3. CLEANUP STATE ---
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glCullFace(GL_BACK); // Restore default

            // ---- END ANIMATION/TRANSPARENCY RENDER LOGIC (VIEW 0) ----


            // --- PICK PASS for view 0 ---
            glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[pickWrite]);
            glViewport(0, 0, w, h);
            glEnable(GL_DEPTH_TEST);
            glClearColor(0, 0, 0, 0); // id=0 => "no hit"
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            pickShader.use();
            pickShader.setMat4("uProj", proj);
            pickShader.setMat4("uView", view);

            for (auto const& entityId : mEntities) {
                auto& transformComponent = ecsInterface.GetComponent<TransformComponent>(entityId);
                glm::mat4 model(1.0f);
                model = glm::translate(model, transformComponent.position);
                model = glm::rotate(model, transformComponent.rotation.x, glm::vec3(1, 0, 0));
                model = glm::rotate(model, transformComponent.rotation.y, glm::vec3(0, 1, 0));
                model = glm::rotate(model, transformComponent.rotation.z, glm::vec3(0, 0, 1));
                model = glm::scale(model, transformComponent.scale);
                pickShader.setMat4("uModel", model);

                uint32_t id = static_cast<uint32_t>(entityId) + 1u;
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
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind pick FBO
            std::swap(pickWrite, pickRead);

            // READBACK for VIEW 0
            if (pendingPickUV0.x >= 0.0f && pendingPickUV0.y >= 0.0f) {
                int pw = 1280, ph = 720; // Use FBO size, not window size if different
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

            // --- BLOOM PASS (for view 0) ---
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

            // --- FINAL PASS (for view 0) ---
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

            // HUD text for view 0
            fontSystem.RenderText2D("Score: 100", 10.f, 10.0f, 1.f,
                glm::vec3(1.0f, 1.0f, 0.5f), (float)w, (float)h);

            // 3D text for view 0
            glEnable(GL_DEPTH_TEST); // Re-enable depth for 3D text
            glm::mat4 textModel = glm::mat4(1.0f);
            textModel = glm::translate(textModel, glm::vec3(0.0f, 5.0f, 0.0f));
            fontSystem.RenderText3D("iDesign Engine by iDesign Studio",
                textModel,
                view, // Use game view
                proj, // Use game proj
                0.05f,
                glm::vec3(1.0f, 1.0f, 0.5f));
            glDisable(GL_DEPTH_TEST); // Disable again for UI
        }
        // ======================= END VIEW 0 ==========================


        // ====================================================================
        // 2. RENDER VIEW 1: SCENE (free-fly)
        // ====================================================================
        {
            // Scene pass -> HDR
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE); // Added

            glViewport(0, 0, w, h);
            glClearColor(0.07f, 0.09f, 0.12f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Camera& cam1 = sceneCamera;
            glm::mat4 proj1 = cam1.getProjectionMatrix((float)w, (float)h);
            glm::mat4 view1 = cam1.getViewMatrix();

            // --- Terrain ---
            terrainShader.use();
            terrainShader.setMat4("uProj", proj1);
            terrainShader.setMat4("uView", view1);
            terrainShader.setMat4("uModel", glm::mat4(1.f));
            terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f });
            LightComponent::upload(terrainShader, sun, hasSun, pointLights, spotLights);
            terrain.draw();

            // ---- START ANIMATION/TRANSPARENCY RENDER LOGIC (VIEW 1) ----

            int sel1 = lastPickedId; // You might want a separate lastPickedId1
            const glm::vec3 kHighlightAlbedo(1.0f, 0.85f, 0.3f);

            std::vector<Entity> opaqueEntities;
            std::multimap<float, Entity> transparentEntities;

            for (auto const& entityId : mEntities)
            {
                if (ecsInterface.HasComponent<MeshProperties>(entityId))
                {
                    auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);

                    if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                    {
                        auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);

                        if (material.opacity < 1.0f)
                        {
                            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                            float distance = glm::distance(tr.position, cam1.position); // Use cam1
                            transparentEntities.insert({ distance, entityId });
                        }
                        else
                        {
                            opaqueEntities.push_back(entityId);
                        }
                    }
                }
            }

            // --- 1. OPAQUE PASS (VIEW 1) ---
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);

            for (auto const& entityId : opaqueEntities)
            {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                glm::mat4 model(1.0f);
                model = glm::translate(model, tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);

                // Text 
                if (ecsInterface.HasComponent<TextProperties>(entityId)) {
                    auto& txt = ecsInterface.GetComponent<TextProperties>(entityId);
                    fontSystem.RenderText3D(txt.text, model, view1, proj1, txt.scale, txt.color);
                }

                // Mesh 
                if (ecsInterface.HasComponent<MeshProperties>(entityId))
                {
                    auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);
                    
                    if (mesh.debugShape == DebugShapeType::None) {

                        bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(entityId);
                        ShaderProgram& currentshader = isSkinned ? skinnedShader : meshShader;

                        currentshader.use();
                        currentshader.setMat4("uProj", proj1);
                        currentshader.setMat4("uView", view1);
                        currentshader.setVec3("uViewPos", cam1.position);
                        currentshader.setMat4("uModel", model);

                        if (isSkinned) {
                            auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                            for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i)
                            {
                                currentshader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                            }
                        }

                        if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                        {
                            auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);
                            const bool isSelected = (int)entityId == sel1;
                            glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : material.albedo;
                            currentshader.setVec3("uAlbedo", finalAlbedo);

                            glActiveTexture(GL_TEXTURE0);
                            currentshader.setVec3("uMaterial.ambient", material.ambient);
                            currentshader.setVec3("uMaterial.specular", material.specular);
                            currentshader.setFloat("uMaterial.shininess", material.shininess);
                            currentshader.setFloat("uMaterial.opacity", material.opacity);
                            currentshader.setInt("uTexture", 0);
                        }
                        
                        if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                            auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                            glBindTexture(GL_TEXTURE_2D, tex.textureID);
                        }
                        else {
                            glBindTexture(GL_TEXTURE_2D, 0);
                        }

                        LightComponent::upload(currentshader, sun, hasSun, pointLights, spotLights);
                        glBindVertexArray(mesh.vao);
                        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                    }
                    else {
                        auto it = m_debugMeshes.find(mesh.debugShape);
                        if (it != m_debugMeshes.end()) {
                            DrawDebugMesh(it->second, mesh.debugcolor, model, proj1, view1);
                        }
                    }
                }
            }

            // --- 2. TRANSPARENT PASS (VIEW 1) ---
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            for (auto it = transparentEntities.rbegin(); it != transparentEntities.rend(); ++it)
            {
                auto const& entityId = it->second;
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);
                auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);

                glm::mat4 model(1.0f);
                model = glm::translate(model, tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);

                if (mesh.debugShape == DebugShapeType::None)
                {
                    bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(entityId);
                    ShaderProgram& currentShader = isSkinned ? skinnedShader : meshShader;

                    currentShader.use();
                    currentShader.setMat4("uProj", proj1);
                    currentShader.setMat4("uView", view1);
                    currentShader.setVec3("uViewPos", cam1.position);
                    currentShader.setMat4("uModel", model);

                    if (isSkinned)
                    {
                        auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                        for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i)
                        {
                            currentShader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                        }
                    }

                    const bool isSelected = (int)entityId == sel1;
                    glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : material.albedo;
                    currentShader.setVec3("uAlbedo", finalAlbedo);
                    currentShader.setVec3("uMaterial.ambient", material.ambient);
                    currentShader.setVec3("uMaterial.specular", material.specular);
                    currentShader.setFloat("uMaterial.shininess", material.shininess);
                    currentShader.setFloat("uMaterial.opacity", material.opacity);

                    glActiveTexture(GL_TEXTURE0);
                    currentShader.setInt("uTexture", 0);
                    if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                        glBindTexture(GL_TEXTURE_2D, tex.textureID);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }

                    LightComponent::upload(currentShader, sun, hasSun, pointLights, spotLights);
                    glBindVertexArray(mesh.vao);
                    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                }
            }

            // --- 3. CLEANUP STATE ---
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glCullFace(GL_BACK); // Restore default

            // ---- END ANIMATION/TRANSPARENCY RENDER LOGIC (VIEW 1) ---

            // --- PICK PASS for view 1 ---
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
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind pick FBO
            std::swap(pickWrite, pickRead);

            // READBACK for VIEW 1
            if (pendingPickUV1.x >= 0.0f && pendingPickUV1.y >= 0.0f) {
                int pw = 1280, ph = 720; // Use FBO size
                int px = glm::clamp(int(pendingPickUV1.x * pw), 0, pw - 1);
                int py = glm::clamp(int(pendingPickUV1.y * ph), 0, ph - 1);

                glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[pickRead]);
                unsigned char rgb[3] = { 0,0,0 };
                glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                uint32_t id = (uint32_t)rgb[0] | ((uint32_t)rgb[1] << 8) | ((uint32_t)rgb[2] << 16);
                lastPickedId = (id == 0) ? -1 : int(id) - 1; // Or set lastPickedId1
                pendingPickUV1 = { -1.0f, -1.0f };
            }


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

            // 3D world-space text
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
        }
        // ===================== END VIEW 1 : SCENE (free-fly) ========================


        // This pick UV logic seems to be for a single, non-view-specific pick
        // You may need to remove this if you only use pendingPickUV0 and pendingPickUV1
        if (pendingPickUV.x >= 0.0f && pendingPickUV.y >= 0.0f) {
            int pw = 1280, ph = 720;
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


        //Showing FPS
        double currentTime = glfwGetTime();
        double delta = currentTime - m_fpsLastTime;
        m_frameCount++;

        if (delta >= 1.0)
        {
            double fps = static_cast<double>(m_frameCount) / delta;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << fps << " FPS";
            glfwSetWindowTitle(window, ss.str().c_str());
            m_frameCount = 0;
            m_fpsLastTime = currentTime;
        }

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
        const bool editorActive = (m_mouseTargetView == 1);   // 0 = game, 1 = editor
        Camera& cam = editorActive ? sceneCamera : gameCamera;

        // ============================================================
        // EDITOR CAMERA SWIVEL (only when holding right-mouse)
        // ============================================================
        if (editorActive)
        {
            static bool firstMouse = true;
            static double lastX = 0.0, lastY = 0.0;

            // Check if right-mouse is held
            int rightMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
            if (rightMouseState == GLFW_PRESS)
            {
                // Lock cursor for smooth rotation

                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);

                if (firstMouse)
                {
                    lastX = xpos;
                    lastY = ypos;
                    firstMouse = false;
                }

                float xoffset = static_cast<float>(xpos - lastX);
                float yoffset = static_cast<float>(lastY - ypos); // reversed Y
                lastX = xpos;
                lastY = ypos;

                // Rotate the editor (scene) camera
                cam.processMouseMovement(xoffset, yoffset);
            }
            else
            {
                // When released: reset mouse state and free the cursor
                firstMouse = true;
            }
        }

        // ============================================================
        // (Optional) movement � WASD or fly movement
        // ============================================================
        const bool useGround = (!editorActive) && groundMode;
        const float baseSpeed = editorActive ? 5.0f : 1.0f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Forward, dt * baseSpeed, useGround, terrain);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Backward, dt * baseSpeed, useGround, terrain);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Left, dt * baseSpeed, useGround, terrain);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cam.processKeyboard(Camera::Movement::Right, dt * baseSpeed, useGround, terrain);

        if (!useGround)
        {
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Up, dt * baseSpeed, false, terrain);
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Down, dt * baseSpeed, false, terrain);
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
        if (dir == "W") camera.processKeyboard(Camera::Movement::Forward, dt, gm, terrain);
        else if (dir == "S") camera.processKeyboard(Camera::Movement::Backward, dt, gm, terrain);
        else if (dir == "A") camera.processKeyboard(Camera::Movement::Left, dt, gm, terrain);
        else if (dir == "D") camera.processKeyboard(Camera::Movement::Right, dt, gm, terrain);
    }

    void Graphics::NudgeLightFromLua(const std::string& axis, float amount) {
        if (pointLights.empty()) return;
        if (axis == "up")    pointLights[0].position.z -= amount;
        else if (axis == "down")  pointLights[0].position.z += amount;
        else if (axis == "left")  pointLights[0].position.x -= amount;
        else if (axis == "right") pointLights[0].position.x += amount;
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

    int Graphics::ConsumeLastPickedId()
    {
        int id = lastPickedId;
        lastPickedId = -1;
        return id;
    }

    glm::mat4 Graphics::GetViewMatrix(int viewIdx) const
    {
        const Camera& cam = (viewIdx == 1) ? sceneCamera : gameCamera;
        return cam.getViewMatrix();
    }

    glm::mat4 Graphics::GetProjectionMatrix(int viewIdx, float w, float h) const
    {
        const Camera& cam = (viewIdx == 1) ? sceneCamera : gameCamera;
        return cam.getProjectionMatrix(w, h);
    }

    Camera* Graphics::GetCameraByIndex(int viewIdx)
    {
        return (viewIdx == 1) ? &sceneCamera : &gameCamera;
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