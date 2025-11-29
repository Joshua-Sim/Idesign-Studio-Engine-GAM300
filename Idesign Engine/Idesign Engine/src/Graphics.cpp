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
#include "Graphics.h"
#include <algorithm>
#include "EngineState.h"
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
#include "AnimationLoader.h"
#include <ScriptProperties.h>
#include <BoxColliderComponent.h>
#include "EventManager.h"
#include <RecipeBookManager.h>
#include "GameSystem.h"
#include "ItemComponent.h"
#include "Inventory.h"
#include "PhaseManager.h"

extern Framework::FontSystem fontSystem;
extern Framework::Coordinator ecsInterface;

const int   TerrainSize = 128;
const float TerrainScale = 1.0f;
Terrain     terrain{ TerrainSize, TerrainScale };
Graphics* g_GraphicsSingleton = nullptr;
static std::unordered_map<std::string, GLuint> s_iconCache;

namespace Framework {
    extern Framework::PhaseManager* phaseManager;
    Graphics* Graphics::s_instance = nullptr;
    bool Graphics::mouseCaptured = true;
    static bool firstMouse = true;
    static bool mouseCaptured = true;
    bool Graphics::groundMode = true; // toggle with a key (e.g., F1)
    bool Graphics::s_RenderDebugColliders = false;
    bool GameSystem::bookOpen = false;
    const auto& assets = AssetManager::Instance().GetAssets();
    bool Graphics::IsCameraRotationEnabled() const { return m_cameraRotationEnabled; }

    // Constructor
    Graphics::Graphics()
    {
        g_GraphicsSingleton = this;
        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>(AssetManager::Instance().FindGuid<WindowConfig>());
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return; }
        s_instance = this;
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
        shadowShader.destroy();
        pointShadowShader.destroy();
        if (m_shadowFBO) glDeleteFramebuffers(1, &m_shadowFBO);
        if (m_shadowDepthTexture) glDeleteTextures(1, &m_shadowDepthTexture);
        if (m_treeRenderer) { delete m_treeRenderer; m_treeRenderer = nullptr; }
        if (m_leafRenderer) { delete m_leafRenderer; m_leafRenderer = nullptr; }
        if (m_grassRenderer) { delete m_grassRenderer; m_grassRenderer = nullptr; }

        // [FIX] DELETE THE PEBBLE/PINE RENDERERS
        // Assuming PebbleRenderer works the same way
        if (m_pebbleRenderer) { delete m_pebbleRenderer; m_pebbleRenderer = nullptr; }
        if (m_pineRenderer) { delete m_pineRenderer; m_pineRenderer = nullptr; }

        // [FIX] DELETE THE LEAF MESH QUAD
        // You created this with 'new' in Initialize: leafQuad = new Mesh(...)
        if (leafQuad) {
            leafQuad->cleanupGL();
            delete leafQuad;
            leafQuad = nullptr;
        }
        // Finally, destroy GLFW window + terminate GLFW
        if (window) {
            glfwSetFramebufferSizeCallback(window, nullptr); // stop resize callback
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }

    void Graphics::SetCameraRotationEnabled(bool enabled)
    {
        m_cameraRotationEnabled = enabled;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    // Initialize the system
    void Graphics::Initialize()
    {
        // asset load / loading shape units
       // Testing (Edwin)
        AssetManager::Instance().LoadBinaryArray<AnimationProperties>("Resource/AnimationAsset.bin", g_AnimationPropertiesProps, [](AnimationProperties& props)
            {
                AnimationLoader::loadAnimation(props.path, props);
            });
        AssetManager::Instance().LoadBinaryArray<TextureProperties>("Resource/TextureAsset.bin", g_TexturePropertiesProps, [](TextureProperties& props)
            {
                TextureLoader::loadTexture(props, true);
            });

        AssetManager::Instance().LoadBinaryArray<MeshProperties>("Resource/MeshAsset.bin", g_MeshProps, [](MeshProperties& props)
            {
                auto animGUID = AssetManager::Instance().FindGuid<AnimationProperties>(props.name);
                Framework::AnimationProperties* animProps = nullptr;

                if (!animGUID.empty()) {
                    animProps = AssetManager::Instance().Get<AnimationProperties>(animGUID);
                }

                // If we found a corresponding animation asset, use the animated loader
                if (animProps)
                {
                    //std::cout << "[Graphics Init] Found anim data for mesh '" << props.name << "'. Loading as animated." << std::endl;

                    // *** Pass the animProps (with skeleton) to the animated mesh loader ***
                    MeshLoader::loadAnimatedMesh(props.path, *animProps, props);
                }
                else
                {
                    // No anim data found, load as a static mesh
                    //std::cout << "[Graphics Init] No anim data for mesh '" << props.name << "'. Loading as static." << std::endl;
                    MeshLoader::loadMesh(props.path, props);
                }
            });

        ImguiManager::Log("Initializing Graphics System...", "Graphics");

        WindowConfig* config = AssetManager::Instance().Get<WindowConfig>(AssetManager::Instance().FindGuid<WindowConfig>());

        // always enable for depth test framebuffering
        glEnable(GL_DEPTH_TEST);

        //// set callbacks
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

        //// IMPORTANT: always store Graphics* as user pointer, not Camera*
        glfwSetWindowUserPointer(window, this);
        glfwSetScrollCallback(window, scroll_callback);

        // Register Graphics related Component
        ImguiManager::Log("Registering graphics components...", "Graphics");
        ecsInterface.RegisterComponent<TransformComponent>();
        ecsInterface.RegisterComponent<HierarchyComponent>(); // Register in graphics as its default generic component allocated to all entities.
        ecsInterface.RegisterComponent<MeshProperties>();
        ecsInterface.RegisterComponent<TextureProperties>();
        ecsInterface.RegisterComponent<TextProperties>();
        ecsInterface.RegisterComponent<NavMeshComponent>();
        ecsInterface.RegisterComponent<NavMeshAgentComponent>(); // Move to navmesh sys after fixing load scene for mesh model
        ecsInterface.RegisterComponent<TagComponent>(); // Engine specific default component
        ecsInterface.RegisterComponent<ListenerComponent>();
        ecsInterface.RegisterComponent<AudioBankMetadata>();
        ecsInterface.RegisterComponent<PhysicsBodyComponent>();
        ecsInterface.RegisterComponent<MaterialProperties>();
        ecsInterface.RegisterComponent<MaterialMetadata>();
        ecsInterface.RegisterComponent<LightProperties>();
        ecsInterface.RegisterComponent<PhysicsProperties>();

        ecsInterface.RegisterComponent<BoxColliderComponent>();
        ecsInterface.RegisterComponent<ItemComponent>();
        ecsInterface.RegisterComponent<ButtonComponent>();

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
        auto meshGUID = AssetManager::Instance().FindGuid<MeshProperties>("knifechoppingwithBONEFIXED4");
        auto animGUID = AssetManager::Instance().FindGuid<AnimationProperties>("knifechoppingwithBONEFIXED4");
      
        
        if (!meshGUID.empty() && !animGUID.empty())
        {
            // 3. Get the actual asset data
            auto* meshAsset = AssetManager::Instance().Get<MeshProperties>(meshGUID);
            auto* animAsset = AssetManager::Instance().Get<AnimationProperties>(animGUID);

            // 4. Add components by COPYING the asset data
            // This is instant - no file loading!
            ecsInterface.AddComponent<MeshProperties>(animatedEntity, *meshAsset);
            ecsInterface.AddComponent<AnimationProperties>(animatedEntity, *animAsset);

            // (You would also add a MaterialProperties component here)
        }

        ecsInterface.AddComponent<MaterialProperties>(animatedEntity, materialComponent);

        Framework::TextureProperties animTexture;
        // --- !! IMPORTANT: Change this path to your model's texture !! ---
        animTexture.path = "Assets/Textures/music.png";
        TextureLoader::loadTexture(animTexture, true);
        ecsInterface.AddComponent<Framework::TextureProperties>(animatedEntity, animTexture);
        // ----------------- End animated object creation ---------------------

        // ====================================================================
        // --- CREATE A TEST LIGHT ENTITY ---
        // ====================================================================
        Entity lightEntity = ecsInterface.CreateEntity();

        // 1. Add a Transform for the light's position
        TransformComponent lightTransform;
        lightTransform.position = glm::vec3(0.0f, 5.0f, 2.0f); // Position it above the scene
        ecsInterface.AddComponent<TransformComponent>(lightEntity, lightTransform);

        // 2. Add the LightProperties component
        LightProperties pointLight;
        pointLight.type = LightType::Point;
        pointLight.color = glm::vec3(1.0f, 1.0f, 1.0f); // White light
        pointLight.intensity = 10.0f; // Make it nice and bright
        pointLight.radius = 20.0f;    // Give it a large radius
        pointLight.enabled = true;
        ecsInterface.AddComponent<LightProperties>(lightEntity, pointLight);

        // 3. Add a Hierarchy component (since your other entities have it)
        ecsInterface.AddComponent<HierarchyComponent>(lightEntity, HierarchyComponent{});

        cube = Mesh::CreateUnitCubeData();
        plane = Mesh::CreateUnitSphereData();
        cone = Mesh::CreateUnitConeData();
        //cylinder = Mesh::CreateUnitCylinderData();
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
        shadowShader.initGL();
        pointShadowShader.initGL();
        uiShader.initGL();
        //initialise geometry objects
        cube.initGL();
        plane.initGL();
        cone.initGL();
        //cylinder.initGL();
        triangle.initGL();
        //FONT SYSTEM

        glGenTextures(1, &m_WhiteTex);
        glBindTexture(GL_TEXTURE_2D, m_WhiteTex);
        unsigned int pixel = 0xFFFFFFFF; // RGBA 255,255,255,255
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);


        m_debugMeshes[DebugShapeType::Box] = &cube;
        m_debugMeshes[DebugShapeType::Sphere] = &plane; // Remember 'plane' is your sphere mesh
        m_debugMeshes[DebugShapeType::Triangle] = &triangle;

        sceneCamera.position = glm::vec3(0.0f, 5.0f, 10.0f); // Or any valid position
        // This forces the camera to update its internal vectors

        //int w = 1280, h = 720;
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        if (w <= 0 || h <= 0) { w = 1280; h = 720; } // safe fallback, rarely used

        m_fbW = w; m_fbH = h;
        m_pickW = w; m_pickH = h;

        InitializeFramebuffers(w, h);

        lastTime = (float)glfwGetTime();

        m_frameCount = 0;
        m_fpsLastTime = glfwGetTime(); // Start the timer

        //IMPORTANT : ALWAYS STICK TO THIS ORDER
        std::vector<std::string> dayFaces =
        {
        "Assets/Textures/SkyNoon_Right.png",    //RIGHT
        "Assets/Textures/SkyNoon_Left.png",    //LEFT 
        "Assets/Textures/SkyNoon_Top.png",    //TOP or UP
        "Assets/Textures/SkyNoon_Bottom.png",     //BOTTOM or DOWN
        "Assets/Textures/SkyNoon_Front.png",    //FRONT
        "Assets/Textures/SkyNoon_Back.png"      //BACK
        };

        std::vector<std::string> nightFaces =
        {
        "Assets/Textures/SkyMidNight_Right.png",    //RIGHT
        "Assets/Textures/SkyMidNight_Left.png",    //LEFT 
        "Assets/Textures/SkyMidNight_Top.png",    //TOP or UP
        "Assets/Textures/SkyMidNight_Bottom.png",     //BOTTOM or DOWN
        "Assets/Textures/SkyMidNight_Front.png",    //FRONT
        "Assets/Textures/SkyMidNight_Back.png"      //BACK
        };

        // Make sure the folder and files exist!
        m_skybox.init(dayFaces, nightFaces);

        //GRASS INIT
        TextureProperties grass;
        grass.path = "Assets/Textures/Grass_normal_left.png";
        grassTextureID = TextureLoader::loadTexture(grass, true);

        // === FOLIAGE/INSTANCING INIT ===
        // 1. Create a simple mesh to represent the tree (Cylinder for a trunk)
        // NOTE: If you have a way to load a real tree model (e.g., from an OBJ file 
        // using MeshLoader), use that instead!
        m_foliageMesh = Mesh::CreateUnitCylinderData(12);
        m_foliageMesh.initGL(); // Upload the cylinder data to the GPU VBO/VAO

        // ===================== LEAF SYSTEM =====================
        leafQuad = new Mesh(Mesh::CreateBillboardQuad());
        leafQuad->initGL();

        // Load your leaf texture (PNG with alpha)
        TextureProperties leafProps;
        leafProps.path = "Assets/Textures/unchopped_herb.png";
        leafTexture = TextureLoader::loadTexture(leafProps, true);

        if (!leafTexture)
            std::cout << "Leaf PNG failed to load!" << std::endl;

        // === 1. LOAD TREE TEXTURE ===
        TextureProperties treeTexProps;
        treeTexProps.path = "Assets/Textures/Wood.png"; // <-- CHANGE PATH TO YOUR TEXTURE!

        // Assuming TextureLoader::loadTexture is available (from your uploaded files)
        // NOTE: loadTexture takes TextureProperties& props and bool flip
        m_treeTextureID = TextureLoader::loadTexture(treeTexProps, true);

        if (m_treeTextureID == 0) {
            std::cerr << "[FoliageRenderer] WARNING: Tree texture failed to load! Check path: " << treeTexProps.path << std::endl;
        }

        // 2. Create the Instancing Renderer
        // TRUNK RENDERER (cylinder)
        m_treeRenderer = new FoliageRenderer(
            &m_foliageMesh,
            "Assets/shaders/instanced_vert.glsl",
            "Assets/shaders/instanced_frag.glsl"
        );

        // LEAF RENDERER (billboards)
        m_leafRenderer = new FoliageRenderer(
            leafQuad,
            "Assets/shaders/instancedleave_vert.glsl",
            "Assets/shaders/instancedleave_frag.glsl"
        );
        //// 2. Create the Instancing Renderer

        // 3. Generate positions for the trees around the border
        std::vector<glm::mat4> leafTransforms;
        std::vector<glm::mat4> trunkTransforms;
        const int NUM_TREES = 4000; // High number for a dense border

        // Calculate map boundaries (Assuming terrain starts at 0,0)
        // If your terrain is centered at (0,0), change min/max accordingly.
        // Range is now -64 to +64
        float mapLimit = ((float)TerrainSize * TerrainScale) * 0.5f;

        float minMap = -mapLimit;
        float maxMap = mapLimit;

        float borderThickness = 20.0f; // How thick the forest border is

        for (int i = 0; i < NUM_TREES; i++) {
            float x = minMap + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (maxMap - minMap)));
            float z;

            // Randomly choose one of the 4 sides
            // 0 = Bottom Edge, 1 = Top Edge, 2 = Left Edge, 3 = Right Edge
            int side = rand() % 4;

            // Helper to get a random float between 0.0 and 1.0
            float r1 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            float r2 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

            if (side == 0) {
                // Bottom Edge: X is anywhere, Z is in the small border strip (0 to 20)
                x = minMap + (r1 * (maxMap - minMap));
                z = minMap + (r2 * borderThickness);
            }
            else if (side == 1) {
                // Top Edge: X is anywhere, Z is at the far end (108 to 128)
                x = minMap + (r1 * (maxMap - minMap));
                z = maxMap - (r2 * borderThickness);
            }
            else if (side == 2) {
                // Left Edge: Z is anywhere, X is in the small border strip
                x = minMap + (r2 * borderThickness);
                z = minMap + (r1 * (maxMap - minMap));
            }
            else {
                // Right Edge: Z is anywhere, X is at the far end
                x = maxMap - (r2 * borderThickness);
                z = minMap + (r1 * (maxMap - minMap));
            }

            // --- Standard Height/Scale Logic ---
            // Get height from terrain so trees stick to the ground
            float y = terrain.getHeightAt(x, z);

            // translation
            glm::mat4 leaves(1.0f);
            leaves = glm::translate(leaves, glm::vec3(x, y, z));

            // Encode width + height in matrix
            leaves[0][3] = 3.0f; // width
            leaves[1][3] = 4.5f; // height

            leafTransforms.push_back(leaves);

            // Random rotation (Y-axis) so trees don't all look identical
            float randomRot = (static_cast<float>(rand()) / RAND_MAX) * 360.0f;
            glm::mat4 trunk = glm::translate(glm::mat4(1.f), glm::vec3(x, y, z));
            trunk = glm::rotate(trunk, glm::radians(randomRot), glm::vec3(0, 1, 0));
            trunk = glm::scale(trunk, glm::vec3(3.0f, 2.5f, 0.12f));

            trunkTransforms.push_back(trunk);
        }

        // after the for-loop that fills treeTransforms
        m_treeRenderer->setInstances(trunkTransforms);
        m_leafRenderer->setInstances(leafTransforms);

        // GENERATE PEBBLE MESH
        // === LOAD PEBBLE MESH ===
        //=== LOAD PEBBLE MESH ===
        MeshLoader::loadMesh("Assets/Objects/rocks.obj", m_pebbleMesh);

        // Load texture for pebbles
        TextureProperties pebProps;
        pebProps.path = "Assets/Textures/T_Brick_BaseColor.png";
        pebbleTextureID = TextureLoader::loadTexture(pebProps, true);

        // Create renderer
        m_pebbleRenderer = new PebbleRenderer(&m_pebbleMesh);

        // === Generate Pebble Transforms ===
        std::vector<glm::mat4> pebbleTransforms;
        const int NUM_PEBBLES = 1000;

        auto rnd = [] { return (float)rand() / (float)RAND_MAX; };

        // Center rectangle to block
        float blockMinX = -10.f;
        float blockMaxX = 10.f;
        float blockMinZ = -10.f;
        float blockMaxZ = 10.f;

        for (int i = 0; i < NUM_PEBBLES; i++)
        {
            float x, z;

            // Keep generating until the rock is OUTSIDE the blocked region
            do {
                x = rnd() * (maxMap - minMap) + minMap;
                z = rnd() * (maxMap - minMap) + minMap;
            } while (x >= blockMinX && x <= blockMaxX &&
                z >= blockMinZ && z <= blockMaxZ);

            float y = terrain.getHeightAt(x, z);

            glm::mat4 M(1.0f);
            M = glm::translate(M, glm::vec3(x, y + 0.02f, z));

            float rot = rnd() * 360.0f;
            M = glm::rotate(M, glm::radians(rot), glm::vec3(0, 1, 0));

            float s = 0.05f + rnd() * 0.8f;
            M = glm::scale(M, glm::vec3(s));

            pebbleTransforms.push_back(M);
        }

        m_pebbleRenderer->setInstances(pebbleTransforms);


        // GENERATE PINES MESH
         // === LOAD PINES MESH ===
        //=== LOAD PINES MESH ===
        MeshLoader::loadMesh("Assets/Objects/pine.obj", m_pineMesh);

        // Load texture for pine
        TextureProperties pineProps;
        pineProps.path = "Assets/Textures/treeLong.png";
        pineTextureID = TextureLoader::loadTexture(pineProps, true);

        // Create renderer
        m_pineRenderer = new PebbleRenderer(&m_pineMesh);

        // === Generate Pebble Transforms ===
        std::vector<glm::mat4> pineTransform;
        const int NUM_PINE = 3000;

        for (int i = 0; i < NUM_PINE; i++)
        {
            float x, z;

            // Keep generating until the rock is OUTSIDE the blocked region
            do {
                x = rnd() * (maxMap - minMap) + minMap;
                z = rnd() * (maxMap - minMap) + minMap;
            } while (x >= blockMinX && x <= blockMaxX &&
                z >= blockMinZ && z <= blockMaxZ);

            float y = terrain.getHeightAt(x, z);

            glm::mat4 M(1.0f);
            M = glm::translate(M, glm::vec3(x, y + 0.02f, z));

            float rot = rnd() * 360.0f;
            M = glm::rotate(M, glm::radians(rot), glm::vec3(0, 1, 0));

            float s = 0.05f + rnd() * 0.8f;
            M = glm::scale(M, glm::vec3(s));

            pineTransform.push_back(M);
        }

        m_pineRenderer->setInstances(pineTransform);


        ImguiManager::LogSuccess("Graphics System initialized successfully", "Graphics");
    }

    void Graphics::SetImguiManager(ImguiManager* manager)
    {
        m_imguiManager = manager;
    }
    // In Graphics.cpp
    // REPLACE your entire Update function with this:

    void Graphics::Update(float deltaTime)
    {
        (void)deltaTime;

        if (glfwWindowShouldClose(window)) {
            CORE->EndGameLoop(); // Mark the game loop as done
            return;
        }

        if (m_resizePending)
        {
            m_resizePending = false;

            // Only resize if dimensions are valid
            if (m_newWidth > 0 && m_newHeight > 0)
            {
                // Updates internal size variables
                m_fbW = m_newWidth;
                m_fbH = m_newHeight;

                // Call your existing resize logic safely
                OnResize(m_fbW, m_fbH);

                // Call ImGui resize safely
                if (m_imguiManager) {
                    m_imguiManager->ResizeViewFBOs(m_fbW, m_fbH);
                }
            }
        }

        float now = (float)glfwGetTime();
        float dt = now - lastTime; lastTime = now;
        process_input(dt);

        // ====================================================================
        // 0. LIGHT GATHERING PASS
        // ====================================================================
        Framework::DirectionalLightCPU frameSun;
        bool frameHasSun = false;
        std::vector<Framework::PointLightCPU> framePointLights;
        std::vector<Framework::SpotLightCPU>  frameSpotLights;
        GatherLights(frameSun, frameHasSun, framePointLights, frameSpotLights);

        // Get window size and check for resize
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w == 0 || h == 0) return;

        if (w != m_fbW || h != m_fbH) {
            m_fbW = w; m_fbH = h;
            OnResize(m_fbW, m_fbH);
            if (m_imguiManager) m_imguiManager->ResizeViewFBOs(m_fbW, m_fbH);
        }

        // ====================================================================
        // 0.5. SHADOW PASS (Render from Sun's perspective)
        // ====================================================================
        if (frameHasSun)
        {
            // 1. Calculate the Light's View/Projection matrix
            float near_plane = 1.0f, far_plane = 200.0f;
            float ortho_size = 100.0f;
            glm::mat4 lightProjection = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);
            //glm::vec3 lightPos = frameSun.position - frameSun.direction * 20.0f;
            glm::vec3 lookTarget = glm::vec3(0.0f, 0.0f, 0.0f);

            // 2. Position the light *relative to the target*, far enough back to see everything
            //    (frameSun.direction is the vector the light shines *along*)
            float shadow_distance = 80.0f; // Pulls the camera back
            glm::vec3 lightPos = lookTarget - frameSun.direction * shadow_distance;

            // 3. Create the view matrix
            glm::mat4 lightView = glm::lookAt(lightPos, lookTarget, glm::vec3(0.0f, 1.0f, 0.0f));
            m_lightSpaceMatrix = lightProjection * lightView;
            // 2. Bind the shadow shader and FBO
            shadowShader.use();
            shadowShader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);

            // 3. Render all shadow-casting objects
            glDisable(GL_CULL_FACE);
            // Render Terrain
            shadowShader.setMat4("uModel", glm::mat4(1.0f));
            terrain.draw();

            for (auto const& entityId : mEntities)
            {
                if (ecsInterface.HasComponent<MeshProperties>(entityId) &&
                    ecsInterface.HasComponent<TransformComponent>(entityId))
                {
                    // 1. Get the MESH component
                    auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);
                    bool isTransparent = false;

                    // 2. Check for a MATERIAL component to determine opacity
                    if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                    {
                        auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);
                        isTransparent = material.opacity < 1.0f;
                    }
                    // 3. Draw based on the MESH component's data
                    if (mesh.debugShape == DebugShapeType::None && mesh.vao != 0 && !isTransparent)
                    {
                        auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                        glm::mat4 model(1.0f);
                        model = glm::translate(model, tr.position);
                        model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                        model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                        model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                        model = glm::scale(model, tr.scale);

                        shadowShader.setMat4("uModel", model);
                        glBindVertexArray(mesh.vao);
                        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                    }
                }
            }
            // 4. Restore state
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glCullFace(GL_BACK);
        }

        // ====================================================================
        //  POINT LIGHT SHADOW PASSES 
        // ====================================================================
        glDisable(GL_CULL_FACE); 
        pointShadowShader.use();

        const unsigned int POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;

        for (auto const& entityId : mEntities)
        {
            if (!ecsInterface.HasComponent<LightProperties>(entityId)) continue;

            auto& light = ecsInterface.GetComponent<LightProperties>(entityId);
            if (light.type != LightType::Point || !light.castsShadow) continue;

            auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);

            // Store the far plane (radius) for the shader
            light.shadowFarPlane = light.radius;
            pointShadowShader.setFloat("uFarPlane", light.shadowFarPlane);
            pointShadowShader.setVec3("uLightPos", transform.position);

            if (light.shadowFBO == 0)
            {
                glGenFramebuffers(1, &light.shadowFBO);
                glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);

                // Create the CUBEMAP to store linear depth (as a color buffer)
                glGenTextures(1, &light.shadowDepthCubeMap);
                glBindTexture(GL_TEXTURE_CUBE_MAP, light.shadowDepthCubeMap);
                for (unsigned int i = 0; i < 6; ++i)
                {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R32F,
                        POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
                }
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

                // Create a DEPTH buffer for depth testing *during* this pass
                glGenRenderbuffers(1, &light.shadowDepthRBO);
                glBindRenderbuffer(GL_RENDERBUFFER, light.shadowDepthRBO);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                    POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER, light.shadowDepthRBO);

                // Attach one face so we can check FBO status
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X, light.shadowDepthCubeMap, 0);
                glDrawBuffer(GL_COLOR_ATTACHMENT0); // We are writing to the color buffer
                glReadBuffer(GL_NONE);

                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    std::cout << "ERROR: Point light Shadow FBO is not complete!" << std::endl;

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            // --- 2. Calculate the 6 View matrices and 1 Projection matrix ---
            float near_plane = 0.1f;
            glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f),
                (float)POINT_SHADOW_WIDTH / (float)POINT_SHADOW_HEIGHT,
                near_plane, light.shadowFarPlane);

            std::vector<glm::mat4> shadowTransforms;
            glm::vec3 lightPos = transform.position;
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0))); // +X
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0))); // -X
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0))); // +Y
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0))); // -Y
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0))); // +Z
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0))); // -Z

            // --- 3. Render the scene 6 times ---
            glViewport(0, 0, POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);
            glEnable(GL_DEPTH_TEST);

            for (unsigned int i = 0; i < 6; ++i)
            {
                // Attach the correct cubemap face for rendering
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, light.shadowDepthCubeMap, 0);

                // Clear *both* the color (our distance) and depth (for testing)
                glClearColor(FLT_MAX, 0.0, 0.0, 1.0); // Clear distance to max
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                pointShadowShader.setMat4("uLightSpaceMatrix", shadowTransforms[i]);

                // --- Render the entire scene (terrain + entities) ---
                pointShadowShader.setMat4("uModel", glm::mat4(1.0f));
                terrain.draw();

                for (auto const& entityId_inner : mEntities)
                {
                    if (ecsInterface.HasComponent<MeshProperties>(entityId_inner) &&
                        ecsInterface.HasComponent<TransformComponent>(entityId_inner))
                    {

                        if (entityId_inner == entityId)
                        {
                            continue;
                        }
                        auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId_inner);
                        bool isTransparent = false;
                        if (ecsInterface.HasComponent<MaterialProperties>(entityId_inner))
                        {
                            isTransparent = ecsInterface.GetComponent<MaterialProperties>(entityId_inner).opacity < 1.0f;
                        }
                        if (mesh.debugShape == DebugShapeType::None && mesh.vao != 0 && !isTransparent)
                        {
                            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId_inner);
                            glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
                            model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                            model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                            model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                            model = glm::scale(model, tr.scale);

                            pointShadowShader.setMat4("uModel", model);
                            glBindVertexArray(mesh.vao);
                            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                        }
                    }
                }
            }
        } 

        // --- 4. Restore default state ---
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ====================================================================
        // 1. RENDER VIEW 0: GAME (grounded)
        // ====================================================================
        if (groundMode)
        {
            //float groundY = terrain.getHeightAt(gameCamera.position.x, gameCamera.position.z);
            gameCamera.position.y = 20.0f;
            for (auto const& entityId : mEntities) {
                if (ecsInterface.GetEntityName(entityId) == "Player") {
                    if (ecsInterface.HasComponent<TransformComponent>(entityId)) {
                        auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);
                        gameCamera.position = transform.position;
                       
                    }
                }
            }
        }
        RenderView(
            gameCamera, 0, w, h,
            frameSun, frameHasSun, framePointLights, frameSpotLights
        );

        // ====================================================================
        // 2. RENDER VIEW 1: SCENE (free-fly)
        // ====================================================================
        RenderView(
            sceneCamera, 1, w, h,
            frameSun, frameHasSun, framePointLights, frameSpotLights
        );

        // ====================================================================
        // 3. FINAL/SHARED LOGIC
        // ====================================================================

        if (pendingPickUV.x >= 0.0f && pendingPickUV.y >= 0.0f) {
            int pw = m_pickW, ph = m_pickH;
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
        if (auto* gfx = static_cast<Graphics*>(glfwGetWindowUserPointer(window)))
        {
            // DO NOT call OnResize here. It causes the crash.
            // Just flag it for the next frame.
            gfx->m_resizePending = true;
            gfx->m_newWidth = w;
            gfx->m_newHeight = h;
        }
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
    
    void Graphics::process_input(float dt)
    {
        if (!engineState.IsPlay()) return;

        // 1. Determine which mode we are in
        // If UI is focused (Editor Mode), we control Scene Camera (Index 1)
        // If Game is focused, we control Game Camera (Index 0)
        // We check InputHandler directly for the source of truth
        bool isEditorMode = InputHandler::GetInstance()->isUIFocused;

        // ============================================================
        // LOGIC 1: EDITOR MODE (Scene Camera Movement)
        // ============================================================
       // ============================================================
    // LOGIC 1: EDITOR MODE (Scene Camera Movement)
    // ============================================================
        if (isEditorMode)
        {
            Camera& cam = sceneCamera;
            const float baseSpeed = 5.0f;

            // Keyboard Movement
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Forward, dt * baseSpeed, false, terrain);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Backward, dt * baseSpeed, false, terrain);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Left, dt * baseSpeed, false, terrain);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                cam.processKeyboard(Camera::Movement::Right, dt * baseSpeed, false, terrain);
        
        }
        // ============================================================
        // LOGIC 2: GAME MODE (Player/Game Camera Movement)
        // ============================================================
        
        //else
        //{
        //    // --- 2.1 MOUSE ROTATION REMOVED ---
        //    // (Handled by InputHandler::CursorPositionCallback)

        //    // --- 2.2 PLAYER MOVEMENT & PHYSICS ---
        //    Entity player = ecsInterface.GetEntityByName("Player");

        //    if (ecsInterface.IsEntityValid(player) && ecsInterface.HasComponent<PhysicsProperties>(player))
        //    {
        //        auto& phys = ecsInterface.GetComponent<PhysicsProperties>(player);
        //        auto& trans = ecsInterface.GetComponent<TransformComponent>(player);

        //        // Calculate Direction based on Game Camera
        //        glm::vec3 camForward = gameCamera.front;
        //        camForward.y = 0.0f;
        //        camForward = glm::normalize(camForward);

        //        glm::vec3 camRight = gameCamera.right;
        //        camRight.y = 0.0f;
        //        camRight = glm::normalize(camRight);

        //        glm::vec3 targetVelocity(0.0f);
        //        float moveSpeed = 5.0f;

        //        // Only move if NOT locked in Minigame
        //        if (m_cameraRotationEnabled)
        //        {
        //            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) targetVelocity += camForward;
        //            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) targetVelocity -= camForward;
        //            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) targetVelocity -= camRight;
        //            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) targetVelocity += camRight;
        //        }

        //        if (glm::length(targetVelocity) > 0.1f) {
        //            targetVelocity = glm::normalize(targetVelocity) * moveSpeed;
        //        }

        //        // Physics Application
        //        if (m_cameraRotationEnabled)
        //        {
        //            phys.linearVelocity.x = targetVelocity.x;
        //            phys.linearVelocity.z = targetVelocity.z;
        //            phys.gravityFactor = 1.0f;
        //        }
        //        else
        //        {
        //            // STIRRING MINIGAME: Freeze Player Physics
        //            phys.linearVelocity = glm::vec3(0.0f);
        //            phys.gravityFactor = 0.0f;
        //        }

        //        phys.startAwake = true;

        //        // Snap Camera to Player
        //        glm::vec3 eyeOffset(0.0f, 3.0f, 0.0f);
        //        gameCamera.position = trans.position + eyeOffset;
        //    }

        //    // --- 2.3 GAMEPLAY TOGGLES (Recipe Book, etc) ---
            {
                bool isRPressed = (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS);
                static bool wasRPressed = false;
                if (isRPressed && !wasRPressed) GameSystem::bookOpen = !GameSystem::bookOpen;
                wasRPressed = isRPressed;
            }
        //}

        // Global Hotkeys (HDR/Bloom)
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) hdrExposure += 1.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) if (hdrExposure > 0.0f) hdrExposure -= 1.0f * dt;
    }

    void Graphics::MoveCameraFromLua(const std::string& dir, float dt, bool gm)
    {
        // Use 'gameCamera' so scripts control the actual player view
        if (dir == "W") gameCamera.processKeyboard(Camera::Movement::Forward, dt, gm, terrain);
        else if (dir == "S") gameCamera.processKeyboard(Camera::Movement::Backward, dt, gm, terrain);
        else if (dir == "A") gameCamera.processKeyboard(Camera::Movement::Left, dt, gm, terrain);
        else if (dir == "D") gameCamera.processKeyboard(Camera::Movement::Right, dt, gm, terrain);
        else if (dir == "E") gameCamera.processKeyboard(Camera::Movement::Up, dt, gm, terrain);   // Optional
        else if (dir == "Q") gameCamera.processKeyboard(Camera::Movement::Down, dt, gm, terrain); // Optional
    }
   void Graphics::NudgeLightFromLua(const std::string& axis, float amount) {
        //if (pointLights.empty()) return;
        //if (axis == "up")    pointLights[0].position.z -= amount;
        //else if (axis == "down")  pointLights[0].position.z += amount;
        //else if (axis == "left")  pointLights[0].position.x -= amount;
        //else if (axis == "right") pointLights[0].position.x += amount;
    }
   void Graphics::SetGroundMode(bool state)
   {
       groundMode = state;
       // Optional: You might want to reset camera pitch/yaw here if switching modes
       if (state) {
           // Logic when switching TO ground mode
       }
   }
    void Graphics::ToggleMainLightFromLua() {
      /*  if (pointLights.empty()) return;
        pointLights[0].enabled = !pointLights[0].enabled;*/
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

    void Graphics::OnResize(int w, int h)
    {
        if (w <= 0 || h <= 0) return;

        // 0) Record
        m_fbW = w; m_fbH = h;
        m_pickW = w; m_pickH = h;

        // 1) Resize HDR FBO
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);

        // 2) Resize Bloom ping-pong
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        }

        // 3) Resize Pick FBOs (color + depth)
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, pickColor[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_pickW, m_pickH, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glBindRenderbuffer(GL_RENDERBUFFER, pickDepth[i]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_pickW, m_pickH);
        }

        // 4) ImGui view FBOs (if you manage them here)
        if (m_imguiManager) {
            m_imguiManager->ResizeViewFBOs(w, h);
        }
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

    GLuint Graphics::GetOrLoadIconTex(const char* path)
    {
        if (!path || !*path) return 0;
        auto it = s_iconCache.find(path);
        if (it != s_iconCache.end()) return it->second;

        Framework::TextureProperties t{};
        t.path = path;
        TextureLoader::loadTexture(t, true);
        if (t.textureID == 0) {
            std::cout << "[Hotbar] Failed to load icon: " << path << "\n";
            return 0;
        }
        s_iconCache.emplace(path, t.textureID);
        return t.textureID;
    }

    GLuint Graphics::GetOrLoadIconTexFromItemId(int itemId)
    {
        if (itemId < 0) return 0;
        std::string p = Framework::MakeItemIconPath(itemId);
        return GetOrLoadIconTex(p.c_str());
    }

    void Graphics::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
    {
        (void)window;
        (void)xoffset;
        Graphics* g = g_GraphicsSingleton;
        if (!g) return;
        if (g->GetMouseTargetView() != 0) return; // only in game view

        auto& inv = Framework::Inventory::Instance();
        if (yoffset > 0)      inv.ChangeSelected(+1);
        else if (yoffset < 0) inv.ChangeSelected(-1);
    }

    static inline glm::mat4 UiModel(float x, float y, float w, float h) {
        glm::mat4 m(1.0f);
        m = glm::translate(m, glm::vec3(x + w * 0.5f, y + h * 0.5f, 0.0f));
        m = glm::scale(m, glm::vec3(w * 0.5f, h * 0.5f, 1.0f));
        return m;
    }

    void Graphics::DrawUIRect(const glm::vec2& pos, const glm::vec2& size,
        const glm::vec3& color, float alpha)
    {
        uiShader.use();
        uiShader.setMat4("uModel", UiModel(pos.x, pos.y, size.x, size.y));
        uiShader.setVec3("uColor", color);
        uiShader.setFloat("uAlpha", alpha);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_WhiteTex);
        uiShader.setInt("uTexture", 0);
        renderQuad();
    }

    static inline void DrawUIBorder(Graphics* g, float x, float y, float w, float h, float t,
        const glm::vec3& color, float alpha)
    {
        g->DrawUIRect({ x,         y + h - t }, { w, t }, color, alpha); // top
        g->DrawUIRect({ x,         y }, { w, t }, color, alpha); // bottom
        g->DrawUIRect({ x,         y }, { t, h }, color, alpha); // left
        g->DrawUIRect({ x + w - t, y }, { t, h }, color, alpha); // right
    }

    void Graphics::DrawHotbar(int screenW, int screenH)
    {
        glm::mat4 uiProj = glm::ortho(0.f, (float)screenW, 0.f, (float)screenH);
        uiShader.use();
        uiShader.setMat4("uProjection", uiProj);

        const int   slots = Framework::kHotbarSlots;
        const float marginBottom = 24.0f;
        const float gap = 6.0f;
        const float pad = 8.0f;
        const float slotSize = std::round(std::min(screenH * 0.085f, 96.0f));
        const float totalSlots = slots * slotSize + (slots - 1) * gap;

        const float x0 = std::round((screenW - totalSlots) * 0.5f);
        const float y0 = std::round(marginBottom);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // If you ever enable scissor elsewhere, ensure it�s off for UI:
        // glDisable(GL_SCISSOR_TEST);

        // Back panel
        DrawUIRect({ x0 - 12.0f, y0 - 12.0f },
            { totalSlots + 24.0f, slotSize + 24.0f },
            { 0.05f, 0.05f, 0.06f }, 0.85f);

        // Render slots via Inventory
        auto& inv = Framework::Inventory::Instance();
        const int selected = inv.GetSelectedIndex();

        for (int i = 0; i < slots; ++i)
        {
            const float sx = x0 + i * (slotSize + gap);
            const float sy = y0;

            // slot fill + normal border
            DrawUIRect({ sx, sy }, { slotSize, slotSize }, { 0.12f, 0.13f, 0.16f }, 0.95f);
            DrawUIBorder(this, sx, sy, slotSize, slotSize, 2.0f, { 0.20f, 0.22f, 0.27f }, 1.0f);

            // selected border
            if (i == selected)
                DrawUIBorder(this, sx, sy, slotSize, slotSize, 4.0f, { 1.0f, 0.88f, 0.25f }, 1.0f);

            const auto& s = inv.Get(i);

            // icon
            if (s.itemId >= 0) {
                GLuint tex = GetOrLoadIconTexFromItemId(s.itemId);  // uses cache
                if (tex) {
                    const float inner = slotSize - 2 * pad;
                    DrawUISprite(tex, { sx + pad, sy + pad }, { inner, inner }, 1.0f);
                }
            }
        }

        glEnable(GL_DEPTH_TEST);
    }

    void Graphics::DrawUISprite(GLuint tex, const glm::vec2& pos, const glm::vec2& size, float alpha)
    {
        if (!tex) return;
        uiShader.use();
        uiShader.setMat4("uModel", UiModel(pos.x, pos.y, size.x, size.y));
        uiShader.setVec3("uColor", glm::vec3(1.0f));
        uiShader.setFloat("uAlpha", alpha);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        uiShader.setInt("uTexture", 0);
        renderQuad();
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

        DirectionalLightCPU frameSun;
        bool frameHasSun = false;
        std::vector<PointLightCPU> framePointLights;
        std::vector<SpotLightCPU>  frameSpotLights;

        // Call the helper to fill the light lists
        GatherLights(frameSun, frameHasSun, framePointLights, frameSpotLights);

        // --- Terrain ---
        terrainShader.use();
        terrainShader.setMat4("uProj", proj);
        terrainShader.setMat4("uView", view);
        terrainShader.setMat4("uModel", glm::mat4(1.f));
        terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f });
        LightProperties::upload(terrainShader, frameSun, frameHasSun, framePointLights, frameSpotLights);
        terrain.draw();

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

    // In Graphics.cpp

    void Graphics::GatherLights(
        DirectionalLightCPU& outSun,
        bool& outHasSun,
        std::vector<PointLightCPU>& outPointLights,
        std::vector<SpotLightCPU>& outSpotLights)
    {
        // Clear the lists from the previous frame
        outHasSun = false;
        outPointLights.clear();
        outSpotLights.clear();

        // Iterate over all entities managed by the Graphics system
        for (auto const& entity : mEntities)
        {
            // Check if this entity ALSO has a LightComponent
            if (ecsInterface.HasComponent<LightProperties>(entity))
            {
                auto& light = ecsInterface.GetComponent<LightProperties>(entity);
                auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);

                if (!light.enabled)
                {
                    continue;
                }

                // --- Inlined GetForwardVector() ---
                glm::vec3 forward;
                float yaw = glm::radians(transform.rotation.y);
                float pitch = glm::radians(transform.rotation.x);
                forward.x = cos(yaw) * cos(pitch);
                forward.y = sin(pitch);
                forward.z = sin(yaw) * cos(pitch);
                forward = glm::normalize(forward);
                // --- End ---

                switch (light.type)
                {
                case LightType::Directional:
                    if (!outHasSun) // Only one sun
                    {
                        outSun.direction = forward;
                        outSun.color = light.color;
                        outSun.intensity = light.intensity;
                        outSun.position = transform.position;
                        outHasSun = true;
                        outSun.position = transform.position;
                        outSun.shadowBiasConst = light.shadowBiasConst;
                        outSun.shadowBiasFactor = light.shadowBiasFactor;
                        outHasSun = true;
                    }
                    break;

                case LightType::Point:
                    outPointLights.push_back({
                        transform.position,
                        light.color,
                        light.intensity,
                        light.radius,
                        true,// enabled
                        light.castsShadow,
                        light.shadowBiasConst,
                        light.shadowBiasFactor
                        });
                    break;

                case LightType::Spot:
                    outSpotLights.push_back({
                        transform.position,
                        forward, // direction
                        light.color,
                        light.intensity,
                        light.radius,
                        light.innerCutOffAngle,
                        light.outerCutOffAngle,
                        true // enabled
                        });
                    break;
                }
            }
        }
    }

    void Graphics::InitializeFramebuffers(int width, int height)
    {
        // --- 1. PICKING FBO SETUP ---
        // (Moved lambda definition inside)
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

        pickShader.initGL(); // Init the shader used by this FBO
        makePickFBO(width, height, 0);
        makePickFBO(width, height, 1);

        // --- 2. HDR FRAMEBUFFER SETUP ---
        glGenFramebuffers(1, &hdrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glGenTextures(1, &colorBuffer);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL); // Use parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
        glGenRenderbuffers(1, &rboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height); // Use parameters
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "ERROR::FRAMEBUFFER:: HDR Framebuffer is not complete!\n";

        // --- 3. BLOOM (PINGPONG) FRAMEBUFFER SETUP ---
        glGenFramebuffers(2, pingpongFBO);
        glGenTextures(2, pingpongColorbuffers);
        for (unsigned int i = 0; i < 2; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
            glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL); // Use parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "ERROR::FRAMEBUFFER:: Ping-pong FBO is not complete!\n";
        }

        // --- 4. SHADOW MAP FBO SETUP ---
        glGenFramebuffers(1, &m_shadowFBO);
        glGenTextures(1, &m_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
        // Note: Shadow FBO uses its own fixed size
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
            SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowDepthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // Unbind last FBO
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Graphics::RenderView(
        Camera& cam,
        int viewIndex,
        int width, int height,
        const DirectionalLightCPU& sun, bool hasSun,
        const std::vector<PointLightCPU>& points,
        const std::vector<SpotLightCPU>& spots)
    {
        // Get the correct pick UV for this view
        glm::vec2& pickUV = (viewIndex == 0) ? pendingPickUV0 : pendingPickUV1;


        // 1. Calculate the Global Model Matrix
        glm::mat4 mapModel = glm::translate(glm::mat4(1.0f), glm::vec3(-60.0f, 3.0f, 0.0f)); //hard coded values of mapmodel
        mapModel = glm::scale(mapModel, glm::vec3(5.0f, 5.0f, 5.0f));


        // 1. SCENE PASS
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glViewport(0, 0, width, height);
        glClearColor(0.07f, 0.09f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = cam.getProjectionMatrix((float)width, (float)height);
        glm::mat4 view = cam.getViewMatrix();

        // --- Terrain ---
        terrainShader.use();
        // Set grass texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTextureID);
        terrainShader.setInt("uGrassTex", 0);
        terrainShader.setFloat("uGrassTiling", 20.0f);

        terrainShader.setMat4("uProj", proj);
        terrainShader.setMat4("uView", view);
        terrainShader.setMat4("uModel", mapModel);
        terrainShader.setVec3("uAlbedo", { 0.6f,0.4f,0.2f });

        // Bind shadow map for terrain
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
        terrainShader.setInt("uShadowMap", 1);
        terrainShader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);


        LightProperties::upload(terrainShader, sun, hasSun, points, spots);
        int pointShadowUnit = 2;
        int pointShadowCount = 0;
        for (int i = 0; i < (int)points.size(); ++i)
        {
            if (points[i].castsShadow && pointShadowCount < MAX_POINT_LIGHTS)
            {
                GLuint textureID = 0;
                for (auto const& entityId : mEntities)
                {
                    if (!ecsInterface.HasComponent<LightProperties>(entityId)) continue;
                    auto& light = ecsInterface.GetComponent<LightProperties>(entityId);
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);
                    if (light.type == LightType::Point && transform.position == points[i].position)
                    {
                        textureID = light.shadowDepthCubeMap;
                        break;
                    }
                }

                if (textureID != 0)
                {
                    glActiveTexture(GL_TEXTURE0 + pointShadowUnit);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
                    
                    std::string samplerName = "uPointShadowMaps[" + std::to_string(pointShadowCount) + "]";
                    // Make sure to use 'terrainShader' here!
                    terrainShader.setInt(samplerName.c_str(), pointShadowUnit);

                    pointShadowUnit++;
                    pointShadowCount++;
                }
            }
        }
        terrain.draw();

        // === DRAW FOLIAGE HERE  ===
        if (m_treeRenderer && m_treeTextureID != 0) // Only draw if texture is loaded
        {
            m_treeRenderer->draw(view, proj, mapModel, m_treeTextureID);
        }

        // === DRAW PEBBLES HERE  ===
        if (m_pebbleRenderer)
        {
            m_pebbleRenderer->draw(view, proj, mapModel, pebbleTextureID);
        }

        // === DRAW PINES HERE  ===
        if (m_pineRenderer)
        {
            m_pineRenderer->draw(view, proj, mapModel, pineTextureID);
        }



        // --- ENTITY SORTING ---
        int sel = lastPickedId;
        const glm::vec3 kHighlightAlbedo(1.0f, 0.85f, 0.3f);
        std::vector<Entity> opaqueEntities;
        std::multimap<float, Entity> transparentEntities;
        std::vector<Entity> uiEntities;
        std::vector<Entity> uiTextEntities;    // NEW: for HUD text


        //for (auto const& entityId : mEntities)
        //{
        //    if (ecsInterface.HasComponent<MeshProperties>(entityId))
        //    {

        //        bool isUI = false;
        //        if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
        //            isUI = ecsInterface.GetComponent<TextureProperties>(entityId).isUI;
        //        }

        //        if (isUI) {
        //            uiEntities.push_back(entityId); // Add to UI list
        //            continue; 
        //        }
        //        float opacity = 1.0f;
        //        if (ecsInterface.HasComponent<MaterialProperties>(entityId))
        //        {
        //            opacity = ecsInterface.GetComponent<MaterialProperties>(entityId).opacity;
        //        }

        //        if (opacity < 1.0f)
        //        {
        //            if (ecsInterface.HasComponent<TransformComponent>(entityId))
        //            {
        //                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
        //                float distance = glm::distance(tr.position, cam.position);
        //                transparentEntities.insert({ distance, entityId });
        //            }
        //        }
        //        else
        //        {
        //            opaqueEntities.push_back(entityId);
        //        }
        //    }
        //}
        for (auto const& entityId : mEntities)
        {
            // check if its a ui or tex
            bool hasTex = ecsInterface.HasComponent<TextureProperties>(entityId);
            bool isUI = false;

            if (hasTex)
            {
                auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                isUI = tex.isUI;
            }

            if (isUI)
            {
                
                uiEntities.push_back(entityId);
                continue;
            }
            // Non-UI objects must have a mesh to be rendered in 3D passes
            if (!ecsInterface.HasComponent<MeshProperties>(entityId))
                continue;

            float opacity = 1.0f;
            if (ecsInterface.HasComponent<MaterialProperties>(entityId))
            {
                opacity = ecsInterface.GetComponent<MaterialProperties>(entityId).opacity;
            }

            if (opacity < 1.0f)
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

        // --- 1. OPAQUE PASS ---
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        for (auto const& entityId : opaqueEntities)
        {
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);

            glm::vec3 drawScale = tr.scale;

            glm::mat4 model(1.0f);
            model = glm::translate(model, tr.position);
            model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
            model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
            model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
            model = glm::scale(model, tr.scale);

            // BOXCOLLIDER COLLISION (NEEDA MOVE IT OUTTA GRAPHICS :SKULL:)
            if (mesh.debugShape == DebugShapeType::Box && ecsInterface.HasComponent<BoxColliderComponent>(entityId))
            {
                auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(entityId);
                for (const auto& entry : colliderComp.colliders)
                {
                    glm::mat4 currentShapeModel = model;
                    currentShapeModel = glm::translate(currentShapeModel, entry.center);
                    currentShapeModel = glm::scale(currentShapeModel, entry.size);
                }
            }

            if (ecsInterface.HasComponent<TextProperties>(entityId))
            {
                auto& txt = ecsInterface.GetComponent<TextProperties>(entityId);

                // If it's UI text, register it for later (HUD pass)
                if (txt.screenSpace)
                {
                    uiTextEntities.push_back(entityId);

                    // DO NOT render 3D text for UI elements
                    // DO NOT return/continue because this entity may still have a mesh to render
                }
                else
                {
                    // WORLD-SPACE 3D TEXT
                    glm::mat4 textModel(1.0f);
                    textModel = glm::translate(textModel, tr.position);

                    if (txt.isBillboard)
                    {
                        // Force rotation to face camera
                        textModel[0][0] = view[0][0]; textModel[0][1] = view[1][0]; textModel[0][2] = view[2][0];
                        textModel[1][0] = view[0][1]; textModel[1][1] = view[1][1]; textModel[1][2] = view[2][1];
                        textModel[2][0] = view[0][2]; textModel[2][1] = view[1][2]; textModel[2][2] = view[2][2];
                    }

                    // Apply entity's scale
                    textModel = glm::scale(textModel, tr.scale);

                    // Render 3D text now
                    fontSystem.RenderText3D(
                        txt.text,
                        textModel,
                        view,
                        proj,
                        txt.scale,
                        txt.color,
                        txt.alignment,
                        txt.opacity
                    );
                }
            }

            if (ecsInterface.HasComponent<MeshProperties>(entityId))
            {
                auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);

                if (mesh.vao != 0) {
                    bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(entityId);
                    ShaderProgram& currentshader = isSkinned ? skinnedShader : meshShader;

                    currentshader.use();
                    currentshader.setMat4("uProj", proj);
                    currentshader.setMat4("uView", view);
                    currentshader.setVec3("uViewPos", cam.position);
                    currentshader.setMat4("uModel", model);

                    // Bind shadow map for meshes
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
                    currentshader.setInt("uShadowMap", 1);
                    currentshader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);

                    int pointShadowUnit = 2;
                    int pointShadowCount = 0;

                    // 'points' is the std::vector<PointLightCPU> passed into RenderView
                    for (int i = 0; i < (int)points.size(); ++i)
                    {
                        // The 'points' vector is already packed with only enabled lights.
                        // We just need to check if this enabled light also casts a shadow.
                        if (points[i].castsShadow && pointShadowCount < MAX_POINT_LIGHTS)
                        {
                            // We need to find the *actual* LightProperties component
                            // to get the shadowDepthCubeMap texture ID.
                            // This is a bit tricky, but we can search by position.

                            // This is inefficient, but will prove the fix.
                            // A better long-term fix is to add the textureID to PointLightCPU.
                            GLuint textureID = 0;
                            for (auto const& entityId : mEntities)
                            {
                                if (!ecsInterface.HasComponent<LightProperties>(entityId)) continue;
                                auto& light = ecsInterface.GetComponent<LightProperties>(entityId);
                                auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);

                                // Check if this is the same light
                                if (light.type == LightType::Point && transform.position == points[i].position)
                                {
                                    textureID = light.shadowDepthCubeMap;
                                    break;
                                }
                            }

                            if (textureID != 0)
                            {
                                glActiveTexture(GL_TEXTURE0 + pointShadowUnit);
                                glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

                                // 'pointShadowCount' will now match the shader's 'i' index
                                std::string samplerName = "uPointShadowMaps[" + std::to_string(pointShadowCount) + "]";
                                currentshader.setInt(samplerName.c_str(), pointShadowUnit);

                                pointShadowUnit++;
                                pointShadowCount++;
                            }
                        }
                    }


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
                        glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : material.albedo;
                        currentshader.setVec3("uAlbedo", finalAlbedo);

                        glActiveTexture(GL_TEXTURE0);
                        currentshader.setVec3("uMaterial.ambient", material.ambient);
                        currentshader.setVec3("uMaterial.specular", material.specular);
                        currentshader.setFloat("uMaterial.shininess", material.shininess);
                        currentshader.setFloat("uMaterial.opacity", material.opacity);
                        currentshader.setInt("uTexture", 0);
                    }
                    // START ADDITION: Reset to defaults if no material component exists
                    else
                    {
                        // Default Albedo: White (so textures look normal)
                        const bool isSelected = (int)entityId == sel;
                        glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : glm::vec3(1.0f, 1.0f, 1.0f);

                        currentshader.setVec3("uAlbedo", finalAlbedo);

                        // Default Material settings
                        currentshader.setVec3("uMaterial.ambient", glm::vec3(0.1f));
                        currentshader.setVec3("uMaterial.specular", glm::vec3(0.5f));
                        currentshader.setFloat("uMaterial.shininess", 32.0f);
                        currentshader.setFloat("uMaterial.opacity", 1.0f);
                        currentshader.setInt("uTexture", 0);
                    }

                    glActiveTexture(GL_TEXTURE0);
                    if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                        glBindTexture(GL_TEXTURE_2D, tex.textureID);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }

                    LightProperties::upload(currentshader, sun, hasSun, points, spots);
                    glBindVertexArray(mesh.vao);
                    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                }
                else if (mesh.debugShape != DebugShapeType::None) {
                    auto it = m_debugMeshes.find(mesh.debugShape);
                    if (it != m_debugMeshes.end()) {
                        DrawDebugMesh(it->second, mesh.debugcolor, model, proj, view);
                    }
                }
            }
            // 2. SEPARATE DEBUG RENDERING FOR COLLIDER
            if (Framework::Graphics::s_RenderDebugColliders && ecsInterface.HasComponent<BoxColliderComponent>(entityId))
            {
                auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(entityId);

                for (const auto& entry : colliderComp.colliders)
                {
                    // Start with Identity
                    glm::mat4 colliderModel = glm::mat4(1.0f);

                    // --- STEP 1: APPLY ENTITY TRANSFORM ---
                    // Move to Entity World Position
                    colliderModel = glm::translate(colliderModel, tr.position);

                    // Apply Entity Rotation
                    colliderModel = glm::rotate(colliderModel, glm::radians(tr.rotation.x), { 1,0,0 });
                    colliderModel = glm::rotate(colliderModel, glm::radians(tr.rotation.y), { 0,1,0 });
                    colliderModel = glm::rotate(colliderModel, glm::radians(tr.rotation.z), { 0,0,1 });

                    // Apply Entity Scale
                    colliderModel = glm::scale(colliderModel, tr.scale);

                    // --- STEP 2: APPLY COLLIDER LOCAL TRANSFORM ---
                    // Apply Offset (Center) relative to the scaled entity
                    colliderModel = glm::translate(colliderModel, entry.center);

                    // Apply Collider Size
                    // IMPORTANT: Debug meshes are usually 1x1x1 unit cubes.
                    // If entry.size is the FULL width, we scale by that.
                    colliderModel = glm::scale(colliderModel, entry.size);

                    // --- Draw ---
                    if (m_debugMeshes.find(DebugShapeType::Box) != m_debugMeshes.end())
                    {
                        glm::vec3 debugColor = entry.canPassThrough ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

                        DrawDebugMesh(
                            m_debugMeshes[DebugShapeType::Box],
                            debugColor,
                            colliderModel,
                            proj,
                            view
                        );
                    }
                }
            }
        }

        // --- 2. TRANSPARENT PASS ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        // === DRAW SKYBOX HERE ===
        // It uses the depth buffer to only draw on empty pixels
        m_skybox.draw(view, proj);
        // ====================================================

        // Billboard leaves
        if (m_leafRenderer)
        {
            m_leafRenderer->draw(view, proj, mapModel, leafTexture);
        }

        for (auto it = transparentEntities.rbegin(); it != transparentEntities.rend(); ++it)
        {
            auto const& entityId = it->second;
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
            auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);



            glm::mat4 model(1.0f);
            model = glm::translate(model, tr.position);
            model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
            model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
            model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
            model = glm::scale(model, tr.scale);

            if (mesh.debugShape == DebugShapeType::None && mesh.vao != 0)
            {
                bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(entityId);
                ShaderProgram& currentShader = isSkinned ? skinnedShader : meshShader;

                currentShader.use();
                currentShader.setMat4("uProj", proj);
                currentShader.setMat4("uView", view);
                currentShader.setVec3("uViewPos", cam.position);
                currentShader.setMat4("uModel", model);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
                currentShader.setInt("uShadowMap", 1);
                currentShader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);
                int pointShadowUnit = 2; // Start at texture unit 2
                int pointShadowCount = 0;


                // 'points' is the std::vector<PointLightCPU> passed into RenderView
                for (int i = 0; i < (int)points.size(); ++i)
                {
                    // The 'points' vector is already packed with only enabled lights.
                    // We just need to check if this enabled light also casts a shadow.
                    if (points[i].castsShadow && pointShadowCount < MAX_POINT_LIGHTS)
                    {
                        // We need to find the *actual* LightProperties component
                        // to get the shadowDepthCubeMap texture ID.
                        // This is a bit tricky, but we can search by position.

                        // This is inefficient, but will prove the fix.
                        // A better long-term fix is to add the textureID to PointLightCPU.
                        GLuint textureID = 0;
                        for (auto const& entityId : mEntities)
                        {
                            if (!ecsInterface.HasComponent<LightProperties>(entityId)) continue;
                            auto& light = ecsInterface.GetComponent<LightProperties>(entityId);
                            auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);

                            // Check if this is the same light
                            if (light.type == LightType::Point && transform.position == points[i].position)
                            {
                                textureID = light.shadowDepthCubeMap;
                                break;
                            }
                        }

                        if (textureID != 0)
                        {
                            glActiveTexture(GL_TEXTURE0 + pointShadowUnit);
                            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

                            // 'pointShadowCount' will now match the shader's 'i' index
                            std::string samplerName = "uPointShadowMaps[" + std::to_string(pointShadowCount) + "]";
                            currentShader.setInt(samplerName.c_str(), pointShadowUnit);

                            pointShadowUnit++;
                            pointShadowCount++;
                        }
                    }
                }
                if (isSkinned)
                {
                    auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                    for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i)
                    {
                        currentShader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                    }
                }

                // **FIX**: Fetch material from the ECS component
                if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                {
                    auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);
                    const bool isSelected = (int)entityId == sel;
                    glm::vec3 finalAlbedo = isSelected ? kHighlightAlbedo : material.albedo;
                    currentShader.setVec3("uAlbedo", finalAlbedo);
                    currentShader.setVec3("uMaterial.ambient", material.ambient);
                    currentShader.setVec3("uMaterial.specular", material.specular);
                    currentShader.setFloat("uMaterial.shininess", material.shininess);
                    currentShader.setFloat("uMaterial.opacity", material.opacity);
                }

                glActiveTexture(GL_TEXTURE0);
                currentShader.setInt("uTexture", 0);
                if (ecsInterface.HasComponent<TextureProperties>(entityId)) {
                    auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);
                    glBindTexture(GL_TEXTURE_2D, tex.textureID);
                }
                else {
                    glBindTexture(GL_TEXTURE_2D, 0);
                }

                LightProperties::upload(currentShader, sun, hasSun, points, spots);
                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
            }
        }

        // --- 3. CLEANUP STATE ---
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glCullFace(GL_BACK);
        // --- 4. PICK PASS ---
        glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[pickWrite]);
        glViewport(0, 0, width, height);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        pickShader.use();
        pickShader.setMat4("uProj", proj);
        pickShader.setMat4("uView", view);

        for (auto const& entityId : mEntities) {
            if (ecsInterface.HasComponent<MeshProperties>(entityId) && ecsInterface.GetComponent<MeshProperties>(entityId).vao != 0)
            {
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

                auto& meshComponent = ecsInterface.GetComponent<MeshProperties>(entityId);
                glBindVertexArray(meshComponent.vao);
                glDrawElements(GL_TRIANGLES, meshComponent.indexCount, GL_UNSIGNED_INT, 0);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::swap(pickWrite, pickRead);

        // --- 5. PICK READBACK ---
        if (pickUV.x >= 0.0f && pickUV.y >= 0.0f) {
            int pw = m_pickW, ph = m_pickH;
            int px = glm::clamp(int(pickUV.x * pw), 0, pw - 1);
            int py = glm::clamp(int(pickUV.y * ph), 0, ph - 1);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[pickRead]);
            unsigned char rgb[3] = { 0,0,0 };
            glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            uint32_t id = (uint32_t)rgb[0] | ((uint32_t)rgb[1] << 8) | ((uint32_t)rgb[2] << 16);
            lastPickedId = (id == 0) ? -1 : int(id) - 1;
            pickUV = { -1.0f, -1.0f }; // Reset this view's pick request
        }

        // --- 6. BLOOM PASS ---
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

        // --- 7. FINAL PASS ---
        glBindFramebuffer(GL_FRAMEBUFFER, m_imguiManager->GetGameFramebuffer(viewIndex));
        glViewport(0, 0, width, height);
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

        // --- 8. HUD/TEXT PASS ---
        if (viewIndex == 0)
        {
            fontSystem.RenderText2D("", 100.f, 10.0f, 1.f,
                glm::vec3(1.0f, 1.0f, 0.5f), (float)width, (float)height,1);
        }
        else
        {
            fontSystem.RenderText2D("Editor View", 100.f, 10.f, 1.f, { 1,1,0.5f }, (float)width, (float)height,1);
        }

        //----------------------------------------------------
// CROSSHAIR DRAW (only for main viewIndex 0)
//----------------------------------------------------
        if (viewIndex == 0)
        {
            float screenW = static_cast<float>(width);
            float screenH = static_cast<float>(height);

            float cx = screenW * 0.5f;
            float cy = screenH * 0.5f;

            // Overall size of the crosshair (tweak to taste)
            float outerSize = screenH * 0.018f;   // ~1.8% of screen height
            float innerSize = outerSize * 0.55f;  // inner white square smaller

            // Ortho in pixel space
            glm::mat4 crossProj = glm::ortho(0.0f, screenW, 0.0f, screenH);

            uiShader.use();
            uiShader.setMat4("uProjection", crossProj);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Bottom-left positions for the quads
            glm::vec2 outerPos(cx - outerSize * 0.5f, cy - outerSize * 0.5f);
            glm::vec2 innerPos(cx - innerSize * 0.5f, cy - innerSize * 0.5f);

            // Outer black square (slightly translucent)
            DrawUIRect(
                outerPos,
                { outerSize, outerSize },
                glm::vec3(0.0f, 0.0f, 0.0f),
                0.5f
            );

            // Inner white square
            DrawUIRect(
                innerPos,
                { innerSize, innerSize },
                glm::vec3(1.0f, 1.0f, 1.0f),
                0.5f
            );
        }
        //----------------------------------------------------
        // CROSSHAIR DRAW (only for main viewIndex 0)
        //----------------------------------------------------


        constexpr float UI_DESIGN_WIDTH = 1920.0f;
        constexpr float UI_DESIGN_HEIGHT = 1080.0f;

        glm::mat4 uiProj = glm::ortho(0.0f, UI_DESIGN_WIDTH, 0.0f, UI_DESIGN_HEIGHT);

        uiShader.use();
        uiShader.setMat4("uProjection", uiProj);


        std::sort(uiEntities.begin(), uiEntities.end(),
            [](Framework::Entity a, Framework::Entity b)
            {
                // Get transforms to compare Z values
                // Note: We use the global 'ecsInterface' here
                auto& posA = ecsInterface.GetComponent<TransformComponent>(a).position;
                auto& posB = ecsInterface.GetComponent<TransformComponent>(b).position;
                return posA.z < posB.z;
            }
        );
        glDisable(GL_DEPTH_TEST); // UI is always on top
        glEnable(GL_BLEND);       // UI usually has transparency
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (auto const& entityId : uiEntities)
        {
            // 1. Check for required components
            if (!ecsInterface.HasComponent<TransformComponent>(entityId) ||
                !ecsInterface.HasComponent<TextureProperties>(entityId))
                continue;

            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
            auto& tex = ecsInterface.GetComponent<TextureProperties>(entityId);

            // 2. Bind Texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex.textureID);
            uiShader.setInt("uTexture", 0);

            // [FIX 1] RESET COLOR STATE 
            // The inventory system leaves the shader color dark gray. 
            // We must reset it to White (1,1,1) so your normal UI isn't tinted.
            uiShader.setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
            uiShader.setFloat("uAlpha", 1.0f);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, tr.position);

            // [FIX 2] CORRECT SCALE CALCULATION
            // Your renderQuad() is 2x2 (-1 to 1). 
            // You were previously using the raw scale, making everything 2x bigger.
            // Using * 0.5f makes it pixel-perfect relative to screen coordinates.
            model = glm::scale(model, glm::vec3(tr.scale.x * 0.5f, tr.scale.y * 0.5f, 1.0f));

            // Delete the old line below:
            // model = glm::scale(model, glm::vec3(tr.scale.x, tr.scale.y, 1.0f));

            uiShader.setMat4("uModel", model);
            renderQuad();
        }

        // --------------------------------------------
                // HUD/UI TEXT PASS (2D Screen Space) (Jasper)
                // --------------------------------------------
        for (auto e : uiTextEntities)
        {
            auto& txt = ecsInterface.GetComponent<TextProperties>(e);
            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);

            // Normalized coordinates (0.0 to 1.0) -> Screen Coordinates
            float px = glm::clamp(tr.position.x, 0.0f, 1.0f);
            float py = glm::clamp(tr.position.y, 0.0f, 1.0f);

            float screenX = px * (float)width;
            float screenY = py * (float)height;

            fontSystem.RenderText2D(
                txt.text,
                screenX,
                screenY,
                txt.scale,
                txt.color,
                (float)width,
                (float)height,
                txt.alignment,
                txt.opacity// <--- ADD THIS ARGUMENT (0=Left, 1=Center, 2=Right)
            );
        }

        if (viewIndex == 0)
        {
            DrawHotbar(width, height);
        }
        glEnable(GL_DEPTH_TEST);
        glm::mat4 textModel = glm::mat4(1.0f);
        textModel = glm::translate(textModel, glm::vec3(0.0f, 5.0f, 0.0f));
        fontSystem.RenderText3D("iDesign Engine by iDesign Studio", textModel, view, proj, 0.05f, glm::vec3(1.0f, 1.0f, 0.5f), 0);
        glDisable(GL_DEPTH_TEST);
    }
 }