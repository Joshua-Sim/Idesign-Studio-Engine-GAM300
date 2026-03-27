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
#include "GameSystem.h"
#include "ItemComponent.h"
#include "Inventory.h"
#include "PhaseManager.h"
#include "RecipeBookManager.h"
#include "ParticleComponent.h"
#include "EngineState.h"

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

        // PARTICLE (Jasper)
        if (m_particlePixelRenderer) { delete m_particlePixelRenderer; m_particlePixelRenderer = nullptr; }


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
        glfwDestroyWindow(window);
        glfwTerminate(); // This restores the taskbar and resolution!;
    }

    void Graphics::SetCameraRotationEnabled(bool enabled)
    {
        m_cameraRotationEnabled = enabled;
        /*glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);*/
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
                TextureLoader::loadTextureAsync(props, true);
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
        ecsInterface.RegisterComponent<ParticleEmitterComponent>();
        ecsInterface.RegisterComponent<PrefabComponent>();
        ecsInterface.RegisterComponent<TimerComponent>();

        ImguiManager::LogSuccess("Graphics components registered", "Graphics");

        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<TransformComponent>()); //Set mEntities to be populated with MeshComponent for now

        ecsInterface.SetSystemSignature<Graphics>(signature);


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
        skinnedPointShadowShader.initGL();
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

        m_particlePixelRenderer = new FoliageRenderer(
            leafQuad,
            "Assets/shaders/instancedparticle_vert.glsl",
            "Assets/shaders/instancedparticle_frag.glsl"
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
        // --- map midpoint (for left / right split) ---
        float midMap = (minMap + maxMap) * 0.5f;   // RIGHT = z > midMap


        for (int i = 0; i < NUM_PEBBLES; i++)
        {
            float x, z;

            // Keep generating until the rock is OUTSIDE the blocked region
            do {
                x = rnd() * (maxMap - minMap) + minMap;
                z = rnd() * (maxMap - minMap) + minMap;
            } while ((x >= blockMinX && x <= blockMaxX &&
                z >= blockMinZ && z <= blockMaxZ) || (x < midMap));

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
        MeshLoader::loadMesh("Assets/Objects/AUTUMN PINE TREE MED.obj", m_pineMesh);

        // Load texture for pine
        TextureProperties pineProps;
        pineProps.path = "Assets/Textures/mushroom 2.png";
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
            } while ((x >= blockMinX && x <= blockMaxX &&
                z >= blockMinZ && z <= blockMaxZ) || (x < midMap));

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
        glDisableVertexAttribArray(3);
        glDisableVertexAttribArray(4);
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

        TextureLoader::processUploads(); // drain the queue each frame

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
        process_input(deltaTime);

        // ====================================================================
            // OPTIMIZATION PART 1: UPDATE CACHES
            // ====================================================================
            // 1. Rebuild lists (only runs if entities were added/removed)
        RebuildEntityCache();

        // 2. Calculate all matrices once (stored in m_frameModelMatrices)
        BuildFrameModelMatrices();

        // ====================================================================
        // PHASE 0: UPDATE CAMERA POSITION (MUST RUN FIRST)
        // ====================================================================
        // We do this early so the lighting/shadow passes know where the player is.
        for (auto const& entityId : mEntities)
        {
            if (ecsInterface.GetEntityName(entityId) == "Player" && !engineState.Is(Dialogue)) {
                if (ecsInterface.HasComponent<TransformComponent>(entityId)) {
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);
                    gameCamera.position = transform.position;
                }
                break; // Found player, no need to keep searching
            }
        }
        Camera& activeCam = GetActiveCamera();
        PerformFrustumCulling(activeCam, (float)m_fbW, (float)m_fbH);
        // ====================================================================
        // PHASE 1: PREPARE LIGHTS
        // ====================================================================
        Framework::DirectionalLightCPU frameSun;
        bool frameHasSun = false;
        std::vector<Framework::PointLightCPU> framePointLights;
        std::vector<Framework::SpotLightCPU> frameSpotLights;
        GatherLights(frameSun, frameHasSun, framePointLights, frameSpotLights);

        // Check window resize...
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w == 0 || h == 0) return;

        if (w != m_fbW || h != m_fbH) {
            m_fbW = w; m_fbH = h;
            OnResize(m_fbW, m_fbH);
            if (m_imguiManager) m_imguiManager->ResizeViewFBOs(m_fbW, m_fbH);
        }

        // ====================================================================
           // BUILD GLOBAL SHADOW CASTERS LIST FIRST
           // ====================================================================
        std::vector<Entity> shadowCasters;
        for (auto const& e : mEntities) {
            if (!ecsInterface.HasComponent<MeshProperties>(e)) continue;
            if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;
            if (!ecsInterface.GetComponent<TransformComponent>(e).isActive) continue;
            auto& mesh = ecsInterface.GetComponent<MeshProperties>(e);

            // Check for transparency so glass/water doesn't cast solid shadows
            bool isTransparent = false;
            if (ecsInterface.HasComponent<MaterialProperties>(e)) {
                isTransparent = ecsInterface.GetComponent<MaterialProperties>(e).opacity < 1.0f;
            }

            if (mesh.debugShape == DebugShapeType::None && mesh.vao != 0 && !isTransparent) {
                shadowCasters.push_back(e);
            }
        }

        // ====================================================================
        // 0.5. SHADOW PASS (Render from Sun's perspective)
        // ====================================================================
        if (frameHasSun)
        {
            float near_plane = -100.0f, far_plane = 500.0f;
            float ortho_size = 100.0f;
            glm::mat4 lightProjection = glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);

            glm::vec3 lookTarget = gameCamera.position;
            float shadow_distance = 200.0f;
            glm::vec3 lightPos = lookTarget - frameSun.direction * shadow_distance;

            // FIX: Prevent NaN matrix if sun is pointing straight down
            glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
            if (abs(frameSun.direction.y) > 0.999f) {
                upVector = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            glm::mat4 lightView = glm::lookAt(lightPos, lookTarget, upVector);
            m_lightSpaceMatrix = lightProjection * lightView;

            shadowShader.use();
            shadowShader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE); // Ensure we are actually writing to the depth buffer
            glClear(GL_DEPTH_BUFFER_BIT);

            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT); // Changed to BACK to prevent thin flat meshes from disappearing 

            // Render all shadow-casting objects (using the fixed global list!)
            for (auto const& entityId : shadowCasters)
            {
                auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);
                glm::mat4 model = m_frameModelMatrices[entityId];

                shadowShader.setMat4("uModel", model);
                glBindVertexArray(mesh.vao);

                glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        // ====================================================================
     // PHASE 2: POINT LIGHT SHADOW PASS (OPTIMIZED)
     // ====================================================================
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // PERF: 512 costs 4x less fill/VRAM than 1024 with minimal quality loss for interior lights.
        const unsigned int POINT_SHADOW_WIDTH = 1024;
        const unsigned int POINT_SHADOW_HEIGHT = 1024;
        const float LIGHT_RENDER_DISTANCE = 200.0f;
        const float LIGHT_RENDER_DISTANCE_SQ = LIGHT_RENDER_DISTANCE * LIGHT_RENDER_DISTANCE;

        // Collect ALL entities as shadow casters (not just camera-visible ones).
        // Using m_visibleEntities here was wrong - objects behind the camera must still cast shadows.


        // PERF: throttle shadow updates — re-render each light's cubemap only every 2 frames.
        // For static interior geometry this is visually identical but halves shadow-pass cost.
        static int s_shadowFrameCounter = 0;
        s_shadowFrameCounter++;

        for (auto const& entityId : mEntities)
        {
            if (!ecsInterface.HasComponent<LightProperties>(entityId)) continue;

            auto& light = ecsInterface.GetComponent<LightProperties>(entityId);
            if (light.type != LightType::Point || !light.castsShadow) continue;

            auto& transform = ecsInterface.GetComponent<TransformComponent>(entityId);
            glm::vec3 lightPos = transform.position;

            // 1. DISTANCE CHECK
            glm::vec3 diff = lightPos - gameCamera.position;
            float distSq = glm::dot(diff, diff);
            if (distSq > LIGHT_RENDER_DISTANCE_SQ) continue;

            // PERF: stagger updates — skip odd-frame lights on even frames and vice versa.
            // All lights still update every 2 frames; no light is ever fully frozen.
            if ((int(entityId) & 1) != (s_shadowFrameCounter & 1)) continue;

            // 2. INITIALIZATION GUARD (Prevents Memory Leaks)
            if (light.shadowFBO == 0)
            {
                glGenFramebuffers(1, &light.shadowFBO);
                glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);

                glGenTextures(1, &light.shadowDepthCubeMap);
                glBindTexture(GL_TEXTURE_CUBE_MAP, light.shadowDepthCubeMap);
                for (unsigned int i = 0; i < 6; ++i) {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R32F,
                        POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
                }

                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

                glGenRenderbuffers(1, &light.shadowDepthRBO);
                glBindRenderbuffer(GL_RENDERBUFFER, light.shadowDepthRBO);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, light.shadowDepthRBO);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, light.shadowDepthCubeMap, 0);
                // Tell the FBO it has a color attachment (required for R32F cubemap rendering)
                GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
                glDrawBuffers(1, drawBufs);

                // FIX: verify the FBO is valid before ever rendering into it
                GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
                    std::cerr << "[PointShadow] FBO incomplete for entity " << entityId
                    << ", status=0x" << std::hex << fboStatus << std::dec << "\n";

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            // 3. RENDER PASS (6 Faces)
            // FIX: use light.radius directly as the far plane — do NOT clamp to 50.
            // The shadow-pass fragment shader writes (dist / farPlane) into the R32F cubemap.
            // The mesh shader reads it back as (storedValue * uPointShadowFarPlanes[i]).
            // If these two values differ the depth comparison always fails and shadows vanish.
            light.shadowFarPlane = light.radius;
            pointShadowShader.use();

            float near_plane = 0.1f;
            glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near_plane, light.shadowFarPlane);

            // Create the 6 view matrices for the Cube Map
            std::vector<glm::mat4> shadowTransforms;
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

            glViewport(0, 0, POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, light.shadowFBO);
            // --- THE HAMMER: Force exact OpenGL state ---
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);      // Force depth writing
            glDepthFunc(GL_LESS);      // Reset depth function in case UI changed it
            glDisable(GL_CULL_FACE);   // Stop culling from hiding inside-out meshes or planes
            glDisable(GL_BLEND);       // Ensure alpha blending doesn't wash out the depth

            // Safety net: ensure far plane is NEVER zero (near_plane=0.01 already handles this)
            // Do NOT use max(radius, 50) here — it must stay equal to light.radius so the
            // depth stored in the cubemap and the value read back in the shader match exactly.
            for (unsigned int i = 0; i < 6; ++i)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, light.shadowDepthCubeMap, 0);
                glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                // Render shadow casters using the optimized list
                for (auto const& casterId : shadowCasters)
                {
                    if (casterId == entityId) continue; // Don't let the light shadow itself

                    // PERF: skip casters outside this light's radius — they can't possibly
                    // receive or cast a visible shadow for this light.
                    auto& casterTr = ecsInterface.GetComponent<TransformComponent>(casterId);
                    glm::vec3 casterDiff = casterTr.position - lightPos;
                    // Use radius * 1.5 as a loose bound to account for large meshes
                    float casterDistSq = glm::dot(casterDiff, casterDiff);
                    float cullRadius = light.shadowFarPlane * 1.5f;
                    if (casterDistSq > cullRadius * cullRadius) continue;

                    // Build matrix inline
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), casterTr.position);
                    model = glm::rotate(model, glm::radians(casterTr.rotation.x), { 1,0,0 });
                    model = glm::rotate(model, glm::radians(casterTr.rotation.y), { 0,1,0 });
                    model = glm::rotate(model, glm::radians(casterTr.rotation.z), { 0,0,1 });
                    model = glm::scale(model, casterTr.scale);
                    bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(casterId);
                    ShaderProgram& currentShadowShader = isSkinned ? skinnedPointShadowShader : pointShadowShader;

                    currentShadowShader.use();
                    currentShadowShader.setMat4("uLightSpaceMatrix", shadowTransforms[i]);
                    currentShadowShader.setMat4("uModel", model);
                    currentShadowShader.setVec3("uLightPos", lightPos);
                    currentShadowShader.setFloat("uFarPlane", light.shadowFarPlane);

                    glBindVertexArray(ecsInterface.GetComponent<MeshProperties>(casterId).vao);

                    if (isSkinned) {
                        auto& animator = ecsInterface.GetComponent<AnimationProperties>(casterId);
                        for (size_t b = 0; b < animator.finalBoneMatrices.size(); ++b) {
                            // Optimized static string cache usage recommended here
                            std::string boneName = "uBoneMatrices[" + std::to_string(b) + "]";
                            currentShadowShader.setMat4(boneName.c_str(), animator.finalBoneMatrices[b]);
                        }
                    }
                    glDrawElements(GL_TRIANGLES, ecsInterface.GetComponent<MeshProperties>(casterId).indexCount, GL_UNSIGNED_INT, 0);
                }
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_fbW, m_fbH);

        // ====================================================================
        // 1. RENDER VIEW 0: GAME (grounded)
        // ====================================================================
        if (groundMode && !engineState.Is(EngineMode::Dialogue))
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

        // ====================================================================
            // EXCLUSIVE RENDERING (Saves 10 FPS)
            // ====================================================================
        bool renderView0 = true; // Safe defaults
        bool renderView1 = false;

        if (m_imguiManager) {
            renderView0 = m_imguiManager->IsView0Visible();
            renderView1 = m_imguiManager->IsView1Visible();
        }

        // 1. RENDER VIEW 0: GAME (grounded)
        if (renderView0)
        {
            if (groundMode && !engineState.Is(Dialogue))
            {
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
            RenderView(gameCamera, 0, w, h, frameSun, frameHasSun, framePointLights, frameSpotLights);
        }

        // 2. RENDER VIEW 1: SCENE (free-fly)
        if (renderView1)
        {
            RenderView(sceneCamera, 1, w, h, frameSun, frameHasSun, framePointLights, frameSpotLights);
        }
        //// ====================================================================
        //// 3. FINAL/SHARED LOGIC
        //// ====================================================================

        //if (pendingPickUV.x >= 0.0f && pendingPickUV.y >= 0.0f &&
        //    glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        //{
        //    int pw = m_pickW, ph = m_pickH;
        //    int px = glm::clamp(int(pendingPickUV.x * pw), 0, pw - 1);
        //    int py = glm::clamp(int(pendingPickUV.y * ph), 0, ph - 1);

        //    glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[pickRead]);
        //    unsigned char rgb[3] = { 0,0,0 };
        //    glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        //    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        //    uint32_t id = (uint32_t)rgb[0] | ((uint32_t)rgb[1] << 8) | ((uint32_t)rgb[2] << 16);
        //    lastPickedId = (id == 0) ? -1 : int(id) - 1;
        //    pendingPickUV = { -1.0f, -1.0f };
        //    if (lastPickedId >= 0) {
        //        std::cout << "Picked entity: " << lastPickedId << "\n";
        //    }
        //}

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
        // Dynamic lookup: Check the mode *right now* and return the correct pointer
        return (activeCameraMode == CameraMode::Scene) ? &sceneCamera : &gameCamera;
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
        bool nKeyPressed = false;

        if (!engineState.Is(EngineMode::Play) && !engineState.Is(EngineMode::Editor))
            return;
        // 1. Determine which mode we are in
        // If UI is focused (Editor Mode), we control Scene Camera (Index 1)
        // If Game is focused, we control Game Camera (Index 0)
        // We check InputHandler directly for the source of truth
        bool isEditorMode = InputHandler::GetInstance()->isUIFocused;
        bool isRightClickHeld = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        // ============================================================
     // LOGIC 1: EDITOR MODE (Scene Camera Movement)
     // ============================================================
        if (isEditorMode)
        {
            engineState.PushState(EngineMode::Play);
            // [FIX A] FREEZE PLAYER PHYSICS
            // This prevents the player from "sliding" while you are editing
            Entity player = ecsInterface.GetEntityByName("Player");
            if (player != -1 && ecsInterface.IsEntityValid(player))
            {
                if (ecsInterface.HasComponent<PhysicsProperties>(player)) {
                    auto& phys = ecsInterface.GetComponent<PhysicsProperties>(player);
                    // Zero out velocity so they stop moving immediately
                    phys.linearVelocity = glm::vec3(0.0f);
                    phys.gravityFactor = 0.0f; // Optional: Stop gravity in editor if needed
                }
            }

            // [FIX B] BETTER IMGUI INPUT HANDLING
            // We only block movement if ImGui wants text input (typing), 
            // OR if ImGui wants the keyboard AND we are NOT holding Right Click.
            // If we hold Right Click, we assume the user wants to fly, ignoring the UI.
            ImGuiIO& io = ImGui::GetIO();
            bool isTyping = io.WantTextInput;
            bool isImGuiBlocking = io.WantCaptureKeyboard && !isRightClickHeld;

            if (!isTyping && !isImGuiBlocking)
            {
                Camera& cam = sceneCamera;

                // Boost speed if holding Shift
                float speed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 15.0f : 5.0f;

                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                    std::cout << "DEBUG: Moving Forward (W)" << std::endl;
                    cam.processKeyboard(Camera::Movement::Forward, dt * speed, false, terrain);
                }
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                    std::cout << "DEBUG: Moving Backward (S)" << std::endl;
                    cam.processKeyboard(Camera::Movement::Backward, dt * speed, false, terrain);
                }
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                    std::cout << "DEBUG: Moving Left (A)" << std::endl;
                    cam.processKeyboard(Camera::Movement::Left, dt * speed, false, terrain);
                }
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                    std::cout << "DEBUG: Moving Right (D)" << std::endl;
                    cam.processKeyboard(Camera::Movement::Right, dt * speed, false, terrain);
                }
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { // Up
                    std::cout << "DEBUG: Moving Up (E)" << std::endl;
                    cam.processKeyboard(Camera::Movement::Up, dt * speed, false, terrain);
                }
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { // Down
                    std::cout << "DEBUG: Moving Down (Q)" << std::endl;
                    cam.processKeyboard(Camera::Movement::Down, dt * speed, false, terrain);
                }
            }
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

        // ------------------------------------------------------------
        // Backpack toggle (TAB) - only when in game, not editor focused
        // ------------------------------------------------------------
        {
            static bool wasIPressed = false;
            bool isIPressed = (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS);
            auto& inv = Framework::Inventory::Instance();

            if (isIPressed && !wasIPressed/* && inv.GetBackpackLevel() != 0*/)
            {
                const bool isOpen = inv.IsBackpackOpen();

                // If it's open, ALWAYS allow closing
                if (isOpen)
                {
                    inv.ToggleBackpack();

                    // restore game mouse lock
                    InputHandler::GetInstance()->isUIFocused = false;
                }
                else
                {
                    // Only allow opening backpack when in valid gameplay state
                    const bool canOpen =
                        !isEditorMode &&
                        !GameSystem::bookOpen &&
                        !engineState.Is(EngineMode::Paused) &&
                        !engineState.Is(EngineMode::HowToPlay) &&
                        !engineState.Is(EngineMode::Menu) &&
                        !engineState.Is(EngineMode::Settings_Audio) &&
                        !engineState.Is(EngineMode::Settings_Gamma) &&
                        !engineState.Is(EngineMode::Dialogue);

                    if (canOpen)
                    {
                        inv.ToggleBackpack();

                        // enable UI cursor
                        InputHandler::GetInstance()->isUIFocused = true;
                    }
                }
            }

            wasIPressed = isIPressed;
        }

        //}

        // ============================================================
        // LOGIC 2: SKYBOX CHANGE TO MORNING
        // ============================================================

        if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
        {
            if (!nKeyPressed)
            {
                float newVal = m_skybox.getBlendFactor() + (dt * 0.5f); // 2 seconds to fade
                if (newVal > 1.0f) newVal = 1.0f;
                m_skybox.setBlendFactor(newVal);
                nKeyPressed = true;
            }
        }
        else
        {
            nKeyPressed = false;
        }

        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) // 'M' for Morning
        {
            if (!nKeyPressed)
            {
                float newVal = m_skybox.getBlendFactor() - (dt * 0.5f);
                if (newVal < 0.0f) newVal = 0.0f;
                m_skybox.setBlendFactor(newVal);
                nKeyPressed = true;
            }
        }
        else
        {
            nKeyPressed = false;
        }

        // Global Hotkeys (HDR/Bloom)
        // 1. Create a static variable to remember the state from the previous frame
        static bool wasXPressed = false;

        // 2. Get the current state
        bool isXPressed = (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS);

        // 3. Check: Is it pressed NOW, but wasn't pressed BEFORE?
        if (isXPressed && !wasXPressed && !isEditorMode)
        {
            phaseManager->TogglePhase();
        }

        // 4. Update the "was" variable for the next frame
        wasXPressed = isXPressed;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) hdrExposure += 1.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) if (hdrExposure > 0.0f) hdrExposure -= 1.0f * dt;

        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) engineState.PrintStack();
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
        // Tell the InputHandler what we want
        if (Framework::InputHandler::GetInstance()) {
            Framework::InputHandler::GetInstance()->SetUIFocused(!on);
        }
    }

    int Graphics::ConsumeLastPickedId()
    {
        int id = lastPickedId;
        lastPickedId = -1;
        return id;
    }

    void Graphics::SetLookAtHighlightedId(int id)
    {
        mLookAtHighlightedId = id;
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
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

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

        auto& inv = Inventory::Instance();
        const int slots = inv.GetHotbarSlotCount();
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

        // ── Back panel ──────────────────────────────────────────────────────────
        DrawUIRect({ x0 - 12.0f, y0 - 12.0f },
            { totalSlots + 24.0f, slotSize + 24.0f },
            { 0.05f, 0.05f, 0.06f }, 0.85f);

        // ── Hotbar slots ────────────────────────────────────────────────────────
        const int selected = inv.GetSelectedIndex();

        for (int i = 0; i < slots; ++i)
        {
            const float sx = x0 + i * (slotSize + gap);
            const float sy = y0;

            DrawUIRect({ sx, sy }, { slotSize, slotSize }, { 0.12f, 0.13f, 0.16f }, 0.95f);
            DrawUIBorder(this, sx, sy, slotSize, slotSize, 2.0f, { 0.20f, 0.22f, 0.27f }, 1.0f);

            if (i == selected)
                DrawUIBorder(this, sx, sy, slotSize, slotSize, 4.0f, { 1.0f, 0.88f, 0.25f }, 1.0f);

            const auto& s = inv.Get(i);
            if (s.itemId >= 0)
            {
                GLuint tex = GetOrLoadIconTexFromItemId(s.itemId);
                if (tex)
                {
                    const float inner = slotSize - 2 * pad;
                    DrawUISprite(tex, { sx + pad, sy + pad }, { inner, inner }, 1.0f);
                }
            }
        }

        // ── Trash slot — placed immediately right of the hotbar panel ────────────
        //   Hotbar panel right edge = (x0 - 12) + (totalSlots + 24) = x0 + totalSlots + 12
        //   Add a comfortable gap so it sits flush-but-separate.
        const float trashGap = 10.0f;
        const float trashSize = slotSize;
        const float trashX = std::round(x0 + totalSlots + 12.0f + trashGap);
        const float trashY = y0;   // vertically aligned with hotbar slots

        // Slot background (dark red tint to signal "danger zone")
        DrawUIRect({ trashX, trashY }, { trashSize, trashSize },
            { 0.20f, 0.04f, 0.04f }, 0.95f);

        // Normal border
        DrawUIBorder(this, trashX, trashY, trashSize, trashSize,
            2.0f, { 0.60f, 0.10f, 0.10f }, 1.0f);

        // Draw trash.png icon inside the slot
        GLuint trashTex = GetOrLoadIconTex("Assets/Textures/trashbin.png");
        if (trashTex)
        {
            const float inner = trashSize - 2 * pad;
            DrawUISprite(trashTex,
                { trashX + pad, trashY + pad },
                { inner, inner }, 1.0f);
        }

        // Hover highlight when the player is dragging over it
        if (inv.IsDragging())
        {
            GLFWwindow* window = GetWindow();
            if (window)
            {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);
                const float mouseX = (float)mx;
                const float mouseY = (float)(screenH - my);

                const bool hoveringTrash =
                    mouseX >= trashX && mouseX <= trashX + trashSize &&
                    mouseY >= trashY && mouseY <= trashY + trashSize;

                if (hoveringTrash)
                    DrawUIBorder(this, trashX, trashY, trashSize, trashSize,
                        5.0f, { 1.0f, 0.20f, 0.20f }, 1.0f);
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

    void Graphics::DrawBackpack(int screenW, int screenH)
    {
        auto& inv = Framework::Inventory::Instance();
        if (!inv.IsBackpackOpen()) return;

        const int cols = Framework::kBackpackCols;
        const int rows = inv.GetBackpackRowCount();
        if (rows <= 0) return;

        glm::mat4 uiProj = glm::ortho(0.f, (float)screenW, 0.f, (float)screenH);
        uiShader.use();
        uiShader.setMat4("uProjection", uiProj);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float gap = 6.0f;
        const float pad = 8.0f;
        const float slotSize = std::round(std::min(screenH * 0.085f, 96.0f));

        const float gridW = cols * slotSize + (cols - 1) * gap;
        const float gridH = rows * slotSize + (rows - 1) * gap;

        const float hotbarY0 = 24.0f;
        const float hotbarPanelH = slotSize + 24.0f;
        const float marginAboveHotbar = 20.0f;
        const float rightMargin = 24.0f;

        const float panelPad = 12.0f;     // matches your panel padding

        // Panel bottom-left (x0,y0) for the GRID (not including outer panel pad)
        const float x0 = std::round((screenW - gridW) * 0.5);
        const float y0 = std::round(hotbarY0 + hotbarPanelH + marginAboveHotbar);     // same vertical logic

        const float panelX = x0 - panelPad;
        const float panelY = y0 - panelPad;
        const float panelW = gridW + 2 * panelPad;
        const float panelH = gridH + 5 * panelPad;

        mBackpackPanelRect = { panelX, panelY, panelW, panelH, true };

        DrawUIRect({ panelX, panelY }, { panelW, panelH },
            { 0.05f, 0.05f, 0.06f }, 0.90f);

        // Title
        const float titleX = x0 + gridW * 0.5f;
        const float panelTop = y0 + gridH + 4.0f * panelPad;
        const float titleY = panelTop - 3.0f * panelPad;

        const float REF_W = 1920.0f;
        const float REF_H = 1080.0f;
        float resScale = std::min((float)screenW / REF_W, (float)screenH / REF_H);
        float scale = 1.25f * resScale;

        fontSystem.RenderText2D(
            "BACKPACK",
            titleX, titleY,
            1.0f, scale,
            glm::vec3(1.0f, 1.0f, 1.0f),
            (float)screenW, (float)screenH,
            1, 1.0f
        );

        // NOTE: Trash slot has been moved to DrawHotbar — no trash code here anymore.

        // Restore GL state after font system
        uiShader.use();
        uiShader.setMat4("uProjection", uiProj);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_SCISSOR_TEST);

        const int activeSlots = inv.GetBackpackSlotCount();

        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                const int   idx = r * cols + c;
                const float sx = x0 + c * (slotSize + gap);
                const float sy = y0 + (rows - 1 - r) * (slotSize + gap);

                DrawUIRect({ sx, sy }, { slotSize, slotSize }, { 0.12f, 0.13f, 0.16f }, 0.95f);
                DrawUIBorder(this, sx, sy, slotSize, slotSize, 2.0f, { 0.20f, 0.22f, 0.27f }, 1.0f);

                if (idx >= activeSlots) continue;

                const auto& s = inv.GetBackpack(idx);
                if (s.itemId >= 0)
                {
                    GLuint tex = GetOrLoadIconTexFromItemId(s.itemId);
                    if (tex)
                    {
                        const float inner = slotSize - 2 * pad;
                        DrawUISprite(tex, { sx + pad, sy + pad }, { inner, inner }, 1.0f);
                    }
                }
            }
        }

        glEnable(GL_DEPTH_TEST);
    }

    void Graphics::HandleBackpackDragDrop(int screenW, int screenH)
    {
        auto& inv = Framework::Inventory::Instance();
        if (!inv.IsBackpackOpen()) return;

        GLFWwindow* window = GetWindow();
        if (!window) return;

        const int cols = Framework::kBackpackCols;
        const int rows = inv.GetBackpackRowCount();
        const bool hasBackpackGrid = (rows > 0);

        const float gap = 6.0f;
        const float pad = 8.0f;
        const float slotSize = std::round(std::min(screenH * 0.085f, 96.0f));

        const float gridW = cols * slotSize + (cols - 1) * gap;
        const float gridH = hasBackpackGrid ? (rows * slotSize + (rows - 1) * gap) : 0.0f;

        const float hotbarY0 = 24.0f;
        const float hotbarPanelH = slotSize + 24.0f;
        const float marginAboveHotbar = 20.0f;

        const float x0 = std::round((screenW - gridW) * 0.5);
        const float y0 = std::round(hotbarY0 + hotbarPanelH + marginAboveHotbar);

        // Mouse (convert to bottom-left origin)
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        const float mouseX = (float)mx;
        const float mouseY = (float)(screenH - my);

        auto hitTestBackpack = [&]() -> Framework::SlotRef
            {
                if (!hasBackpackGrid) return {};

                if (mouseX < x0 || mouseX > x0 + gridW || mouseY < y0 || mouseY > y0 + gridH)
                    return {};

                float localX = mouseX - x0;
                int c = (int)(localX / (slotSize + gap));
                if (c < 0 || c >= cols) return {};

                float localY = mouseY - y0;
                int rFromBottom = (int)(localY / (slotSize + gap));
                int r = (rows - 1 - rFromBottom);
                if (r < 0 || r >= rows) return {};

                // gap reject
                float cellX = localX - c * (slotSize + gap);
                float cellY = localY - rFromBottom * (slotSize + gap);
                if (cellX > slotSize || cellY > slotSize) return {};

                int idx = r * cols + c;
                if (idx < 0 || idx >= inv.GetBackpackSlotCount()) return {};

                return Framework::SlotRef{ Framework::SlotKind::Backpack, idx };
            };

        auto hitTestHotbar = [&]() -> Framework::SlotRef
            {
                const int hotSlots = inv.GetHotbarSlotCount();
                const float marginBottom = 24.0f;
                const float hotGap = 6.0f;
                const float hotSlotSize = slotSize;

                const float totalSlotsW = hotSlots * hotSlotSize + (hotSlots - 1) * hotGap;
                const float hx0 = std::round((screenW - totalSlotsW) * 0.5f);
                const float hy0 = std::round(marginBottom);

                if (mouseX < hx0 || mouseX > hx0 + totalSlotsW || mouseY < hy0 || mouseY > hy0 + hotSlotSize)
                    return {};

                float localX = mouseX - hx0;
                int i = (int)(localX / (hotSlotSize + hotGap));
                if (i < 0 || i >= hotSlots) return {};

                float cellX = localX - i * (hotSlotSize + hotGap);
                if (cellX > hotSlotSize) return {};

                return Framework::SlotRef{ Framework::SlotKind::Hotbar, i };
            };

        auto hitTestTrash = [&]() -> Framework::SlotRef
            {
                // Must match DrawBackpack() trash position
                const int hotSlots = inv.GetHotbarSlotCount();
                const float hotGap = 6.0f;
                const float hotSlotSize = slotSize;
                const float hotbarTotalW = hotSlots * hotSlotSize + (hotSlots - 1) * hotGap;
                const float hotbarX0 = std::round((screenW - hotbarTotalW) * 0.5f);
                const float hotbarY0Exact = std::round(24.0f);

                float trashSize = slotSize;
                float trashGapFromHotbar = 18.0f;
                float trashX = hotbarX0 + hotbarTotalW + trashGapFromHotbar;
                float trashY = hotbarY0Exact;

                if (mouseX < trashX || mouseX > trashX + trashSize ||
                    mouseY < trashY || mouseY > trashY + trashSize)
                    return {};

                return Framework::SlotRef{ Framework::SlotKind::Trash, 0 };
            };

        auto hitTestAny = [&]() -> Framework::SlotRef
            {
                auto t = hitTestTrash();
                if (t.IsValid()) return t;

                auto b = hitTestBackpack();
                if (b.IsValid()) return b;

                auto h = hitTestHotbar();
                if (h.IsValid()) return h;

                return {};
            };

        static bool prevLDown = false;
        const bool lDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

        if (lDown && !prevLDown)
        {
            auto on = hitTestAny();
            if (on.IsValid()) inv.BeginDrag(on);
        }

        if (!lDown && prevLDown)
        {
            if (inv.IsDragging())
            {
                auto on = hitTestAny();
                if (on.IsValid()) inv.DropOn(on);
                else inv.CancelDrag();
            }
        }
        prevLDown = lDown;

        // --- Draw dragged icon (needs UI state/projection) ---
        if (inv.IsDragging())
        {
            glm::mat4 uiProj = glm::ortho(0.f, (float)screenW, 0.f, (float)screenH);
            uiShader.use();
            uiShader.setMat4("uProjection", uiProj);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            auto dragged = inv.GetDraggedItemCopy();
            if (dragged.itemId >= 0)
            {
                GLuint tex = GetOrLoadIconTexFromItemId(dragged.itemId);
                if (tex)
                {
                    const float inner = slotSize - 2 * pad;
                    DrawUISprite(tex,
                        { mouseX - inner * 0.5f, mouseY - inner * 0.5f },
                        { inner, inner },
                        1.0f);
                }
            }

            glEnable(GL_DEPTH_TEST);
        }
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
        finalShader.setFloat("gamma", m_gamma);
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
                        true,               // enabled
                        light.castsShadow,
                        light.shadowBiasConst,
                        light.shadowBiasFactor,
                        // FIX: carry the actual far plane used when rendering the cubemap
                        // so SetupShadowsForShader can upload it as uPointShadowFarPlanes[i].
                        light.shadowFarPlane
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
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
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
        static std::vector<std::string> boneNames;
        static std::vector<std::string> shadowNames;

        // Pre-calculate the strings once to save CPU time
        if (boneNames.empty()) {
            for (int i = 0; i < 100; ++i) boneNames.push_back("uBoneMatrices[" + std::to_string(i) + "]");
            for (int i = 0; i < 8; ++i) shadowNames.push_back("uPointShadowMaps[" + std::to_string(i) + "]");
        }
        // Get the correct pick UV for this view
        glm::vec2& pickUV = (viewIndex == 0) ? pendingPickUV0 : pendingPickUV1;

        // 1. Calculate the Global Model Matrix
        glm::mat4 mapModel = glm::translate(glm::mat4(1.0f), glm::vec3(-60.0f, 3.0f, 0.0f)); //hard coded values of mapmodel
        mapModel = glm::scale(mapModel, glm::vec3(5.0f, 5.0f, 5.0f));

        // --- NEW: FRAME MATRIX CACHE ---
        std::unordered_map<Entity, glm::mat4> frameModelMatrices;
        for (auto const& entityId : mEntities) {
            if (ecsInterface.HasComponent<TransformComponent>(entityId)) {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);
                frameModelMatrices[entityId] = model;
            }
        }


        // 1. SCENE PASS
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glClearStencil(0);
        glEnable(GL_CULL_FACE);  //  Enable culling for main rendering 
        glCullFace(GL_BACK);     //  Cull back faces (standard)

        glViewport(0, 0, width, height);
        glClearColor(0.07f, 0.09f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

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

                    std::string samplerName = "uPointShadowMaps[" + std::to_string(i) + "]";
                    // Make sure to use 'terrainShader' here!
                    terrainShader.setInt(samplerName.c_str(), pointShadowUnit);

                    pointShadowUnit++;
                    pointShadowCount++;
                }
            }
        }
        terrain.draw();

        //// === DRAW FOLIAGE HERE  ===
        //if (m_treeRenderer && m_treeTextureID != 0) // Only draw if texture is loaded
        //{
        //    m_treeRenderer->draw(view, proj, mapModel, m_treeTextureID);
        //}

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
        int hoverSel = mLookAtHighlightedId;
        const glm::vec3 kHighlightAlbedo(1.0f, 0.85f, 0.3f);

        std::vector<Entity> opaqueEntities;
        std::multimap<float, Entity> transparentEntities;
        std::vector<Entity> uiEntities;
        std::vector<Entity> uiTextEntities;

        // DEFINE YOUR MAX RENDER DISTANCE
        // Objects further than this will be ignored completely
        const float MAX_RENDER_DISTANCE = 10000.0f;

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
            if (ecsInterface.HasComponent<TransformComponent>(entityId))
            {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                float dist = glm::distance(tr.position, cam.position);

                // If too far, SKIP adding it to the render lists
                if (dist > MAX_RENDER_DISTANCE) continue;
            }
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

        // === DRAW SKYBOX HERE ===
        // It uses the depth buffer to only draw on empty pixels
        m_skybox.draw(view, proj);
        // ====================================================


        // --- 1. OPAQUE PASS ---
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        // ============================================================================
          // [OPTIMIZATION] PRE-CALCULATE SHADOW MAPS (Runs once per frame)
          // ============================================================================
          // 1. Find the Texture IDs for all active point lights just ONCE.
        std::vector<GLuint> validShadowTextures;
        validShadowTextures.reserve(points.size());

        for (const auto& point : points)
        {
            GLuint foundID = 0;
            if (point.castsShadow)
            {
                // Perform the slow search here (only happens once per frame now!)
                for (auto const& e : mEntities)
                {
                    if (ecsInterface.HasComponent<LightProperties>(e) &&
                        ecsInterface.HasComponent<TransformComponent>(e))
                    {
                        auto& l = ecsInterface.GetComponent<LightProperties>(e);
                        auto& t = ecsInterface.GetComponent<TransformComponent>(e);

                        // Match specific light by position and type
                        if (l.type == LightType::Point && t.position == point.position)
                        {
                            foundID = l.shadowDepthCubeMap;
                            break;
                        }
                    }
                }
            }
            validShadowTextures.push_back(foundID);

        }

        // 2. Bind Textures & Set Uniforms GLOBALLY for this pass.
        //    This avoids re-binding them thousands of times inside the loop.
        auto SetupShadowsForShader = [&](ShaderProgram& sh) {
            sh.use();
            int baseUnit = 2; // Start at Texture Unit 2 (0=Albedo, 1=DirShadow)

            // FIX: use a compacted 'shadowCount' index so that uPointShadowMaps[k] and
            // uPointLights[k] refer to the same light.
            // LightProperties::upload already packs enabled lights into 0..N-1 slots.
            // We must bind cubemaps using the same compacted counter, not the raw
            // points[] index (which would skip disabled lights and cause a mismatch).

            // Step 1: lock ALL sampler slots first so none default to unit 0.
            for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
                sh.setInt(("uPointShadowMaps[" + std::to_string(i) + "]").c_str(), baseUnit + i);
                sh.setFloat(("uPointShadowFarPlanes[" + std::to_string(i) + "]").c_str(), 0.0f);
            }

            // Step 2: bind actual cubemaps and upload far planes using a compacted counter
            int shadowCount = 0;
            for (size_t i = 0; i < points.size() && shadowCount < MAX_POINT_LIGHTS; ++i) {
                if (!points[i].castsShadow || validShadowTextures[i] == 0) continue;

                glActiveTexture(GL_TEXTURE0 + baseUnit + shadowCount);
                glBindTexture(GL_TEXTURE_CUBE_MAP, validShadowTextures[i]);

                // Upload the exact far plane that was used to write this cubemap
                sh.setFloat(("uPointShadowFarPlanes[" + std::to_string(shadowCount) + "]").c_str(),
                    points[i].shadowFarPlane);

                shadowCount++;
            }
            };

        // Apply these settings to BOTH shaders used in the loop
        SetupShadowsForShader(meshShader);
        SetupShadowsForShader(skinnedShader);

        // Reset active texture to default for the loop
        glActiveTexture(GL_TEXTURE0);
        // ============================================================================
        glStencilMask(0x00);
        for (auto const& entityId : opaqueEntities)
        {
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
            if (!tr.isActive) continue; // Skip render if isactive is false.

            glm::vec3 drawScale = tr.scale;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
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

                    // TEMPORARILY ENABLE BLENDING FOR TEXT
                    // Text needs alpha blending to look correct and not have black/grey boxes.
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    // Optional: Disable depth write so text doesn't occlude things behind it weirdly
                    glDepthMask(GL_FALSE);

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

                    // RESTORE OPAQUE STATE
                    glDepthMask(GL_TRUE);
                    glDisable(GL_BLEND);
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




                    if (isSkinned) {
                        auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                        for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i)
                        {
                            currentshader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                        }
                    }
                    // [Graphics.cpp] - Optimized Material Application
                    if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                    {
                        auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);

                        // 1. Determine base color (Yellow if hovered, otherwise material color)
                        const bool isSelected = ((int)entityId == sel) || ((int)entityId == hoverSel);
                        glm::vec3 finalAlbedo = isSelected ? glm::vec3(1.0f, 0.85f, 0.3f) : material.albedo;

                        currentshader.setVec3("uAlbedo", finalAlbedo);
                        currentshader.setVec3("uMaterial.ambient", material.ambient);
                        currentshader.setVec3("uMaterial.specular", material.specular);
                        currentshader.setFloat("uMaterial.shininess", material.shininess);
                        currentshader.setFloat("uMaterial.opacity", material.opacity); // CRITICAL: Reset opacity
                    }
                    else
                    {
                        // 2. FALLBACK: Reset to safe defaults so the next object isn't "weird"
                        const bool isSelected = ((int)entityId == sel) || ((int)entityId == hoverSel);
                        currentshader.setVec3("uAlbedo", isSelected ? glm::vec3(1.0f, 0.85f, 0.3f) : glm::vec3(1.0f));

                        currentshader.setVec3("uMaterial.ambient", glm::vec3(0.1f));
                        currentshader.setVec3("uMaterial.specular", glm::vec3(0.5f));
                        currentshader.setFloat("uMaterial.shininess", 32.0f);
                        currentshader.setFloat("uMaterial.opacity", 1.0f); // Ensure 100% visible
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
                    bool doOutline = ((int)entityId == hoverSel); // or: == sel, or both
                    if (doOutline) {
                        glStencilFunc(GL_ALWAYS, 1, 0xFF);
                        glStencilMask(0xFF);   // enable stencil writes
                    }

                    glBindVertexArray(mesh.vao);
                    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                    if (doOutline) {
                        glStencilMask(0x00);
                        glStencilFunc(GL_ALWAYS, 0, 0xFF); // Optional: Reset func if needed
                    }
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

        // =========================================================
        // FIXED OUTLINE RENDERER (STENCIL METHOD)
        // =========================================================
        if (hoverSel != -1)
        {
            Entity outlineEntity = (Entity)hoverSel;

            if (ecsInterface.HasComponent<TransformComponent>(outlineEntity) &&
                ecsInterface.HasComponent<MeshProperties>(outlineEntity))
            {
                auto& trO = ecsInterface.GetComponent<TransformComponent>(outlineEntity);
                auto& meshO = ecsInterface.GetComponent<MeshProperties>(outlineEntity);

                if (meshO.debugShape == DebugShapeType::None && meshO.vao != 0)
                {
                    // 1. Calculate Base Model Matrix
                    glm::mat4 baseModel(1.0f);
                    baseModel = glm::translate(baseModel, trO.position);
                    baseModel = glm::rotate(baseModel, glm::radians(trO.rotation.x), { 1,0,0 });
                    baseModel = glm::rotate(baseModel, glm::radians(trO.rotation.y), { 0,1,0 });
                    baseModel = glm::rotate(baseModel, glm::radians(trO.rotation.z), { 0,0,1 });
                    baseModel = glm::scale(baseModel, trO.scale);

                    // 2. Create Scaled Up Matrix (The Outline Thickness)
                    float scaleFactor = 1.03f; // 3% larger
                    glm::mat4 outlineModel = glm::scale(baseModel, glm::vec3(scaleFactor));

                    // 3. Configure Stencil to ONLY draw where the object ISN'T
                    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
                    glStencilMask(0x00); // Do not write to stencil, only read

                    // 4. Disable Depth Test if you want "X-Ray" outlines (visible through walls)
                    //    Keep ENABLED if you want walls to hide the outline.
                    glDisable(GL_DEPTH_TEST);

                    // 5. Select Shader
                    bool isSkinned = ecsInterface.HasComponent<AnimationProperties>(outlineEntity);
                    ShaderProgram& outShader = isSkinned ? skinnedShader : debugShader;
                    outShader.use();

                    // 6. Setup Uniforms (Draw Pure White)
                    outShader.setMat4("projection", proj); // Note: debugShader uses 'projection'
                    outShader.setMat4("view", view);       // debugShader uses 'view'

                    if (isSkinned) {
                        outShader.setMat4("uProj", proj); // Skinned uses uProj
                        outShader.setMat4("uView", view); // Skinned uses uView
                        outShader.setMat4("uModel", outlineModel);

                        // Solid White Material
                        outShader.setVec3("uAlbedo", glm::vec3(1.0f));
                        outShader.setVec3("uMaterial.ambient", glm::vec3(1.0f));
                        outShader.setVec3("uMaterial.specular", glm::vec3(0.0f));
                        outShader.setFloat("uMaterial.shininess", 1.0f);
                        outShader.setFloat("uMaterial.opacity", 1.0f);

                        // Unbind texture
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, m_WhiteTex);
                        outShader.setInt("uTexture", 0);

                        // Upload Bones
                        auto& animator = ecsInterface.GetComponent<AnimationProperties>(outlineEntity);
                        for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i) {
                            outShader.setMat4(("uBoneMatrices[" + std::to_string(i) + "]").c_str(), animator.finalBoneMatrices[i]);
                        }
                    }
                    else {
                        // Static Mesh (Debug Shader)
                        outShader.setMat4("model", outlineModel);
                        outShader.setVec3("color", glm::vec3(1.0f, 1.0f, 0.0f)); // Yellow Outline
                    }

                    // 7. Draw
                    glBindVertexArray(meshO.vao);
                    glDrawElements(GL_TRIANGLES, meshO.indexCount, GL_UNSIGNED_INT, 0);
                    glBindVertexArray(0);

                    // 8. Cleanup
                    glStencilMask(0xFF);
                    glStencilFunc(GL_ALWAYS, 0, 0xFF);
                    glEnable(GL_DEPTH_TEST);
                }
            }
        }

        // IMPORTANT: don’t let stencil affect transparent/UI passes
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glDisable(GL_STENCIL_TEST);

        // =========================================================
          // FIXED PARTICLE SYSTEM RENDERER
          // =========================================================

          // 1. Setup Render State for Particles (Transparent + No Depth Write)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // Don't write to depth, but still test against it

        // 2. Use the Standard Mesh Shader
        meshShader.use();
        meshShader.setMat4("uProj", proj);
        meshShader.setMat4("uView", view);
        meshShader.setVec3("uViewPos", cam.position);

        // Set neutral material for particles
        meshShader.setVec3("uAlbedo", glm::vec3(1.0f));
        meshShader.setVec3("uMaterial.ambient", glm::vec3(1.0f));
        meshShader.setVec3("uMaterial.specular", glm::vec3(0.0f));
        meshShader.setFloat("uMaterial.shininess", 1.0f);
        meshShader.setFloat("uMaterial.opacity", 1.0f);

        // 3. Loop through emitters
        for (Entity e : mEntities)
        {
            if (!ecsInterface.HasComponent<ParticleEmitterComponent>(e) ||
                !ecsInterface.HasComponent<TransformComponent>(e))
                continue;

            auto& emitter = ecsInterface.GetComponent<ParticleEmitterComponent>(e);
            auto& xform = ecsInterface.GetComponent<TransformComponent>(e);

            if (!emitter.active || emitter.particles.empty()) continue;
            if (!m_particlePixelRenderer) continue;

            // Pick texture (fallback to white)
            unsigned int texId = (emitter.textureID != 0) ? emitter.textureID : m_WhiteTex;

            // Build instance matrices (one per alive particle)
            std::vector<glm::mat4> particleMats;
            particleMats.reserve(emitter.particles.size());

            // Camera basis for billboarding
            glm::vec3 camRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
            glm::vec3 camUp = glm::vec3(view[0][1], view[1][1], view[2][1]);

            for (const Particle& p : emitter.particles)
            {
                if (p.lifetime <= 0.0f) continue;

                glm::vec3 worldPos = p.position;
                if (emitter.localSpace)
                {
                    worldPos = xform.position + emitter.emitterOffset + p.position;
                }
                else
                {
                    worldPos = emitter.emitterOffset + p.position;
                }

                glm::mat4 m = glm::translate(glm::mat4(1.0f), worldPos);
                m[0] = glm::vec4(camRight * p.size, 0.0f);
                m[1] = glm::vec4(camUp * p.size, 0.0f);
                m[2] = glm::vec4(glm::cross(camRight, camUp), 0.0f);

                particleMats.push_back(m);
            }

            if (particleMats.empty()) continue;

            m_particlePixelRenderer->setInstances(particleMats);
            m_particlePixelRenderer->draw(view, proj, glm::mat4(1.0f), texId);
        }

        // Restore State
        if (leafQuad) glBindVertexArray(0);
        glDepthMask(GL_TRUE);

        // --- 2. TRANSPARENT PASS ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        for (auto it = transparentEntities.rbegin(); it != transparentEntities.rend(); ++it)
        {
            auto const& entityId = it->second;
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
            auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);

            if (!tr.isActive) continue;

            // Calculate model matrix directly (since we aren't using the cache)
            glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
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

                // --- BIND TEXTURES (The fix for the Rainbow Bug) ---

                // Slot 0: Albedo (The object's actual texture)
                glActiveTexture(GL_TEXTURE0);
                if (ecsInterface.HasComponent<TextureProperties>(entityId))
                    glBindTexture(GL_TEXTURE_2D, ecsInterface.GetComponent<TextureProperties>(entityId).textureID);
                else
                    glBindTexture(GL_TEXTURE_2D, 0);
                currentShader.setInt("uTexture", 0);

                // Slot 1: Sun Shadow Map
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
                currentShader.setInt("uShadowMap", 1);
                currentShader.setMat4("uLightSpaceMatrix", m_lightSpaceMatrix);

                // Slots 2+: Point Light Shadow Maps
                // [OPTIMIZATION] We use the pre-calculated 'validShadowTextures' from the Opaque pass
                // This removes the slow mEntities loop!
                // FIX: use a compacted counter so uPointShadowMaps[k] matches uPointLights[k]
                int unit = 2;
                int shadowCount = 0;
                for (size_t i = 0; i < points.size() && shadowCount < MAX_POINT_LIGHTS; ++i) {
                    if (!points[i].castsShadow || validShadowTextures[i] == 0) continue;
                    glActiveTexture(GL_TEXTURE0 + unit);
                    glBindTexture(GL_TEXTURE_CUBE_MAP, validShadowTextures[i]);
                    // Use shadowCount (compacted) not i (raw) — must match LightProperties::upload packing
                    std::string uniformName = "uPointShadowMaps[" + std::to_string(shadowCount) + "]";
                    currentShader.setInt(uniformName.c_str(), unit);
                    currentShader.setFloat(("uPointShadowFarPlanes[" + std::to_string(shadowCount) + "]").c_str(),
                        points[i].shadowFarPlane);
                    unit++;
                    shadowCount++;
                }

                if (isSkinned) {
                    auto& animator = ecsInterface.GetComponent<AnimationProperties>(entityId);
                    for (size_t i = 0; i < animator.finalBoneMatrices.size(); ++i) {
                        currentShader.setMat4(boneNames[i].c_str(), animator.finalBoneMatrices[i]);
                    }
                }

                // [OPTIMIZATION] REMOVED the redundant LightProperties::upload call here!
                // It's already done at the start of RenderView.

                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
            }
        }

        // --- PARTICLES END   --- (Jasper)
        glStencilMask(0x00);
        for (auto it = transparentEntities.rbegin(); it != transparentEntities.rend(); ++it)
        {

            auto const& entityId = it->second;
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
            auto& mesh = ecsInterface.GetComponent<MeshProperties>(entityId);

            if (!tr.isActive) continue; // Skip pass if isactive is false

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

                // [Graphics.cpp] - Optimized Material Application
                if (ecsInterface.HasComponent<MaterialProperties>(entityId))
                {
                    auto& material = ecsInterface.GetComponent<MaterialProperties>(entityId);

                    // 1. Determine base color (Yellow if hovered, otherwise material color)
                    const bool isSelected = ((int)entityId == sel) || ((int)entityId == hoverSel);
                    glm::vec3 finalAlbedo = isSelected ? glm::vec3(1.0f, 0.85f, 0.3f) : material.albedo;

                    currentShader.setVec3("uAlbedo", finalAlbedo);
                    currentShader.setVec3("uMaterial.ambient", material.ambient);
                    currentShader.setVec3("uMaterial.specular", material.specular);
                    currentShader.setFloat("uMaterial.shininess", material.shininess);
                    currentShader.setFloat("uMaterial.opacity", material.opacity); // CRITICAL: Reset opacity
                }
                else
                {
                    // 2. FALLBACK: Reset to safe defaults so the next object isn't "weird"
                    const bool isSelected = ((int)entityId == sel) || ((int)entityId == hoverSel);
                    currentShader.setVec3("uAlbedo", isSelected ? glm::vec3(1.0f, 0.85f, 0.3f) : glm::vec3(1.0f));

                    currentShader.setVec3("uMaterial.ambient", glm::vec3(0.1f));
                    currentShader.setVec3("uMaterial.specular", glm::vec3(0.5f));
                    currentShader.setFloat("uMaterial.shininess", 32.0f);
                    currentShader.setFloat("uMaterial.opacity", 1.0f); // Ensure 100% visible
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
                bool doOutline = ((int)entityId == hoverSel);

                if (doOutline) {
                    glStencilFunc(GL_ALWAYS, 1, 0xFF);
                    glStencilMask(0xFF); // Enable ONLY for this one object
                }
                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
                if (doOutline) {
                    glStencilMask(0x00);
                    glStencilFunc(GL_ALWAYS, 0, 0xFF); // Optional: Reset func if needed
                }
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

                if (transformComponent.isActive == false) continue; // Skip rendering inactive entities, go to next entity

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
        finalShader.setFloat("gamma", m_gamma);
        renderQuad();

        // --- 8. HUD/TEXT PASS ---
        if (viewIndex == 0)
        {
            fontSystem.RenderText2D("", 100.f, 10.0f, 1.0f, 1.f,
                glm::vec3(1.0f, 1.0f, 0.5f), (float)width, (float)height, 1);
        }
        else
        {
            fontSystem.RenderText2D("Editor View", 100.f, 10.f, 1.0f, 1.f, { 1,1,0.5f }, (float)width, (float)height, 1);
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
        uiShader.setFloat("uGamma", m_gamma / 2.2f); // normalize: 2.2=neutral(1.0), <2.2=darker, >2.2=brighter


        // --------------------------------------------
           // MERGED UI PASS: sprites + text sorted by Z together
           // This ensures tooltip backgrounds (sprites) can occlude text behind them.
           // --------------------------------------------

           // Build a unified list of { z, entity, isText }
        struct UIDrawItem {
            float z;
            Framework::Entity entity;
            bool isText;
        };
        std::vector<UIDrawItem> allUIItems;
        allUIItems.reserve(uiEntities.size() + uiTextEntities.size());

        for (auto e : uiEntities)
            allUIItems.push_back({ ecsInterface.GetComponent<TransformComponent>(e).position.z, e, false });

        for (auto e : uiTextEntities)
            allUIItems.push_back({ ecsInterface.GetComponent<TransformComponent>(e).position.z, e, true });

        // Sort ascending z (low = back, high = front)
        std::sort(allUIItems.begin(), allUIItems.end(),
            [](const UIDrawItem& a, const UIDrawItem& b) { return a.z < b.z; });

        glClear(GL_DEPTH_BUFFER_BIT); // fresh depth for UI z-ordering

        // --- FIXED: Disable depth testing so our Z-sort determines draw order ---
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (auto& item : allUIItems)
        {
            auto& tr = ecsInterface.GetComponent<TransformComponent>(item.entity);
            if (!tr.isActive) continue;

            if (!item.isText)
            {
                // --- SPRITE ---
                if (!ecsInterface.HasComponent<TextureProperties>(item.entity)) continue;
                auto& tex = ecsInterface.GetComponent<TextureProperties>(item.entity);

                uiShader.use();
                uiShader.setMat4("uProjection", uiProj);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex.textureID);
                uiShader.setInt("uTexture", 0);
                uiShader.setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
                uiShader.setFloat("uAlpha", tex.uiAlpha);

                glm::mat4 model = glm::mat4(1.0f);
                glm::vec3 snappedPos = glm::vec3(std::round(tr.position.x), std::round(tr.position.y), tr.position.z);
                model = glm::translate(model, snappedPos);
                model = glm::rotate(model, glm::radians(tr.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(tr.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(tr.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

                // 3. Scale
                model = glm::scale(model, glm::vec3(tr.scale.x * 0.5f, tr.scale.y * 0.5f, 1.0f));
                uiShader.setMat4("uModel", model);
                renderQuad();
            }
            else
            {
                // --- TEXT ---
                // ONLY render the Interaction Prompt on the Main Game View (Index 0)
                if (ecsInterface.GetEntityName(item.entity) == "InteractionPrompt" &&
                    (viewIndex != 0 || GameSystem::bookOpen))
                    continue;

                if (!ecsInterface.HasComponent<TextProperties>(item.entity)) continue;
                auto& txt = ecsInterface.GetComponent<TextProperties>(item.entity);

                float px = glm::clamp(tr.position.x, 0.0f, 1.0f);
                float py = glm::clamp(tr.position.y, 0.0f, 1.0f);
                float screenX = px * (float)width;
                float screenY = py * (float)height;

                const float REF_W = 1920.0f;
                const float REF_H = 1080.0f;
                float resScale = std::min((float)width / REF_W, (float)height / REF_H);
                float effectiveScale = txt.scale * resScale;

                fontSystem.RenderText2D(
                    txt.text,
                    screenX, screenY,
                    tr.position.z,
                    effectiveScale,
                    txt.color,
                    (float)width, (float)height,
                    txt.alignment,
                    txt.opacity
                );

                // fontSystem changes GL state — restore for next sprite
                uiShader.use();
                uiShader.setMat4("uProjection", uiProj);

                // --- FIXED: Ensure it stays disabled when restoring state ---
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
        }
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        // Do not display inventory boxes while in paused/howtoplay/menu state
        if (viewIndex == 0 &&
            !engineState.IsAnyUIOpen() &&
            !GameSystem::bookOpen)
        {
            DrawHotbar(width, height);
        }

        if (Framework::Inventory::Instance().IsBackpackOpen())
        {
            DrawBackpack(width, height);
            HandleBackpackDragDrop(width, height);
        }

        Entity loadingEntity = ecsInterface.GetEntityByName("LoadingUI");

        if (loadingEntity != -1 && ecsInterface.IsEntityValid(loadingEntity))
        {
            auto& tr = ecsInterface.GetComponent<TransformComponent>(loadingEntity);

            // Only draw if the LoadingSystem has it "active" (scale > 0)
            if (tr.scale.x > 0.01f)
            {
                auto& tex = ecsInterface.GetComponent<TextureProperties>(loadingEntity);

                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glDepthMask(GL_TRUE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Use the UI projection (1920x1080)
                uiShader.use();
                uiShader.setMat4("uProjection", glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f));
                uiShader.setVec3("uColor", glm::vec3(1.0f));
                uiShader.setFloat("uAlpha", 1.0f);

                // Draw fullscreen (Center 960,540 with half-extents 960,540)
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(960.0f, 540.0f, 0.0f));
                model = glm::scale(model, glm::vec3(960.0f, 540.0f, 1.0f));

                uiShader.setMat4("uModel", model);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex.textureID);
                uiShader.setInt("uTexture", 0);

                renderQuad(); // Draws the fullscreen quad
            }
        }
        // =========================================================
    }


    // ===========================================================================
// OPTIMIZATION PART 1: HELPER FUNCTIONS
// Paste this at the bottom of Graphics.cpp
// ===========================================================================

    void Framework::Graphics::RebuildEntityCache()
    {
        if (!m_cachedEntities.isDirty) return;

        // Clear existing caches
        m_cachedEntities.meshEntities.clear();
        m_cachedEntities.skinnedEntities.clear();
        m_cachedEntities.uiEntities.clear();
        m_cachedEntities.textEntities.clear();
        m_cachedEntities.lightEntities.clear();
        m_cachedEntities.transparentEntities.clear();

        // Iterate ALL entities only once
        for (Entity e : mEntities)
        {
            // CRITICAL FIX: Validate entity before accessing components
            if (!ecsInterface.IsEntityValid(e))
                continue;

            // Skip invalid/inactive entities early
            if (!ecsInterface.HasComponent<TransformComponent>(e))
                continue;

            auto& transform = ecsInterface.GetComponent<TransformComponent>(e);
            if (!transform.isActive)
                continue;

            // 1. LIGHTS
            if (ecsInterface.HasComponent<LightProperties>(e)) {
                m_cachedEntities.lightEntities.push_back(e);
            }

            // 2. UI ELEMENTS
            if (ecsInterface.HasComponent<TextureProperties>(e)) {
                auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                if (tex.isUI) {
                    m_cachedEntities.uiEntities.push_back(e);
                    continue; // UI is handled separately, stop here
                }
            }

            // 3. TEXT
            if (ecsInterface.HasComponent<TextProperties>(e)) {
                m_cachedEntities.textEntities.push_back(e);
                // Don't continue, text might also have a mesh background
            }

            // 4. MESHES (Static & Skinned)
            if (ecsInterface.HasComponent<MeshProperties>(e))
            {
                auto& meshProps = ecsInterface.GetComponent<MeshProperties>(e);

                // Filter out debug shapes if needed, or keep them
                if (meshProps.vao != 0)
                {
                    // Check transparency
                    bool isTransparent = false;
                    if (ecsInterface.HasComponent<MaterialProperties>(e)) {
                        if (ecsInterface.GetComponent<MaterialProperties>(e).opacity < 1.0f)
                            isTransparent = true;
                    }

                    if (isTransparent) {
                        m_cachedEntities.transparentEntities.push_back(e);
                    }
                    else if (ecsInterface.HasComponent<AnimationProperties>(e)) {
                        m_cachedEntities.skinnedEntities.push_back(e);
                    }
                    else {
                        m_cachedEntities.meshEntities.push_back(e);
                    }
                }
            }
        }

        m_cachedEntities.isDirty = false;
    }
    void Framework::Graphics::BuildFrameModelMatrices()
    {
        m_frameModelMatrices.clear();
        m_frameModelMatrices.reserve(mEntities.size());

        for (auto const& entityId : mEntities)
        {
            // CRITICAL FIX: Validate entity before accessing
            if (!ecsInterface.IsEntityValid(entityId))
                continue;

            if (ecsInterface.HasComponent<TransformComponent>(entityId))
            {
                auto& tr = ecsInterface.GetComponent<TransformComponent>(entityId);
                if (!tr.isActive) continue;

                // Calculate Matrix ONCE per frame
                glm::mat4 model = glm::translate(glm::mat4(1.0f), tr.position);
                model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
                model = glm::scale(model, tr.scale);

                m_frameModelMatrices[entityId] = model;
            }
        }
    }
    // ===========================================================================
// OPTIMIZATION PART 2: FRUSTUM CULLING LOGIC
// ===========================================================================

    void Frustum::ExtractFromMatrix(const glm::mat4& viewProj)
    {
        // Extract frustum planes from view-projection matrix
        for (int i = 0; i < 4; ++i) planes[0][i] = viewProj[i][3] + viewProj[i][0]; // Left
        for (int i = 0; i < 4; ++i) planes[1][i] = viewProj[i][3] - viewProj[i][0]; // Right
        for (int i = 0; i < 4; ++i) planes[2][i] = viewProj[i][3] + viewProj[i][1]; // Bottom
        for (int i = 0; i < 4; ++i) planes[3][i] = viewProj[i][3] - viewProj[i][1]; // Top
        for (int i = 0; i < 4; ++i) planes[4][i] = viewProj[i][3] + viewProj[i][2]; // Near
        for (int i = 0; i < 4; ++i) planes[5][i] = viewProj[i][3] - viewProj[i][2]; // Far

        // Normalize planes
        for (int i = 0; i < 6; ++i) {
            float length = glm::length(glm::vec3(planes[i]));
            planes[i] /= length;
        }
    }

    bool Frustum::IsBoxVisible(const glm::vec3& center, const glm::vec3& halfExtents) const
    {
        for (int i = 0; i < 6; ++i) {
            glm::vec3 normal(planes[i]);
            float dist = glm::dot(normal, center) + planes[i].w;
            float r = glm::dot(glm::abs(normal), halfExtents);
            if (dist < -r) return false; // Fully outside
        }
        return true;
    }

    void Framework::Graphics::PerformFrustumCulling(const Camera& cam, float screenW, float screenH)
    {
        m_visibleEntities.clear();

        // 1. Build Frustum
        glm::mat4 proj = cam.getProjectionMatrix(screenW, screenH);
        glm::mat4 view = cam.getViewMatrix();
        m_frustum.ExtractFromMatrix(proj * view);

        // 2. Check Cache - rebuild if dirty or empty
        if (m_cachedEntities.isDirty ||
            (m_cachedEntities.meshEntities.empty() && m_cachedEntities.skinnedEntities.empty()))
        {
            RebuildEntityCache();
        }

        // 3. Test Static Meshes
        for (Entity e : m_cachedEntities.meshEntities)
        {
            // CRITICAL FIX: Validate entity before accessing component
            if (!ecsInterface.IsEntityValid(e))
                continue;

            if (!ecsInterface.HasComponent<TransformComponent>(e))
                continue;

            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);

            // Additional safety check
            if (!tr.isActive)
                continue;

            // Approximation: scale * 1.5 to handle rotation/corners
            if (m_frustum.IsBoxVisible(tr.position, tr.scale * 1.5f)) {
                m_visibleEntities.push_back(e);
            }
        }

        // 4. Test Skinned Meshes
        for (Entity e : m_cachedEntities.skinnedEntities)
        {
            // CRITICAL FIX: Validate entity before adding
            if (!ecsInterface.IsEntityValid(e))
                continue;

            // For now, just assume visible to avoid animating off-screen characters incorrectly
            m_visibleEntities.push_back(e);
        }
    }
}