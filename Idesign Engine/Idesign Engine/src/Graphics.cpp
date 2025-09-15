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
#include <windows.h>
#include <commdlg.h>
#include "Graphics.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize2.h"
#include <InputHandler.h>
#include "ComponentList.h"
#include "FontSystem.h"
#include "Coordinator.h"
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "EngineState.h"
#include "AssetManager.h"
#include "Debugger.h"

extern Framework::FontSystem fontSystem;
extern Framework::Coordinator ecsInterface;

namespace Framework {

    //initialising containers
    std::vector<Graphics::Model> Graphics::models{};
    std::unordered_map<std::string, Graphics::Model> Graphics::meshes{};
    std::unordered_map<std::string, GLuint> Graphics::textures{};
    unsigned char* Graphics::data{};
    float Graphics::projWidth{};
    float Graphics::projHeight{};
    GLuint Graphics::gameFramebuffer{}, Graphics::gameTexture{}, Graphics::rbo{};
    bool print_out = false;
    ParticleSystem particleSystem(1000, glm::vec2(200, 800));  // Define and initialize the particle system

    //** TESTING PURPOSES **//
    float sprite_x = 1150.f;
    int sprite_no = 5;
    float rotate_left = 0.0f;
    float rotate_right = 0.0f;
    float scale_left = 200.0f;
    float scale_right = 200.0f;

    // Loading of Graphics Shaders
    //std::string const UE_vs = GlobalAssetManager.LoadandStoreShader("src/UE.vert");
    //std::string const UE_vs2 = GlobalAssetManager.LoadandStoreShader("src/UE_Vertex.vert");
    //std::string const UE_fs = GlobalAssetManager.LoadandStoreShader("src/UE.frag");
    //std::string const UE_fs2 = GlobalAssetManager.LoadandStoreShader("src/UE_Tint.frag");

    // edwin
    //auto& textureAssets = GlobalAssetManager.GetAllTextureAssets();
    //auto& audioAssets = GlobalAssetManager.GetAllAudioAssets();
    //auto& entityAssets = GlobalAssetManager.GetAllEntities();
       
    static std::string selectedTextureName;
    static std::string selectedAudioName;
    static std::string filePath{};
    static std::string newTextureName = "";
    static std::string deletionRequested = "";  // Flag to track if deletion is requested

    // Error message and flag for popup
    std::string errorMessage;
    bool showErrorPopup = false;

    static bool modeChanged = false;  // Flag to track if the mode has changed
    static bool modeUpdated = false;  // Flag to track if mode was serialized
    static bool soundTypeChanged = false;
    static std::string selectedMode = "";  // String to store the selected mode
    static int selectedAudioPathIndex = 0;
    static bool showEditModal = false;
    static bool isEditing = false; // Flag to check if edit mode is active
    static char newAudioNameBuffer[256];   // Use a char buffer for ImGui::InputText
    std::unordered_map<std::string, bool> editingStates;
    std::vector<std::string> assetsToDelete;

    std::vector<std::string> audioPathOptions
    {
        "assets/audio/bgm/",
        "assets/audio/soundeffect/"
    };
    static bool pathChanged = false;

    // Declare the map for storing dropped texture files
    //std::map<std::string, TextureAsset::Texture> droppedTextureFiles;
    std::vector<std::pair<std::string, std::string>> updates;

    std::vector<glm::vec2> vertices =
    {
        glm::vec2(-0.5f, -0.5f), // Bottom-left
        glm::vec2(0.5f, -0.5f), // Bottom-right
        glm::vec2(0.5f,  0.5f), // Top-right
        glm::vec2(-0.5f,  0.5f)  // Top-left
    };

    std::vector<glm::vec2> texCoords =
    {
        glm::vec2(0.0f, 0.0f),  // Bottom-left
        glm::vec2(1.0f, 0.0f),  // Bottom-right
        glm::vec2(1.0f, 1.0f),  // Top-right
        glm::vec2(0.0f, 1.0f)   // Top-left
    };

    glm::vec3 color{ 1.0f, 1.0f, 1.0f };

    /*void Coordinator::AddComponentToEntity(Entity entity, const std::string& componentName)
    {
        if (componentName == "MovementComponent")
        {
            mComponentManager->AddComponent(entity, MovementComponent{});

        }
        else if (componentName == "EnemyComponent")
        {
            mComponentManager->AddComponent(entity, EnemyComponent{});
        }

        else if (componentName == "CollisionComponent")
        {
            mComponentManager->AddComponent(entity, CollisionComponent{});
        }

        else if (componentName == "TransformComponent")
        {
            mComponentManager->AddComponent(entity, TransformComponent{});
        }

        else if (componentName == "RenderComponent")
        {
            mComponentManager->AddComponent(entity, RenderComponent{});
        }

        else if (componentName == "LayerComponent")
        {
            mComponentManager->AddComponent(entity, LayerComponent{});
        }

        else if (componentName == "TextComponent")
        {
            mComponentManager->AddComponent(entity, TextComponent{});
        }

        else if (componentName == "PlayerComponent")
        {
            mComponentManager->AddComponent(entity, PlayerComponent{});
        }
    }*/

    // Constructor
    Graphics::Graphics(GraphicsWindows* graphicWindows) : graphicWindows(graphicWindows) 
    {
        projHeight = static_cast<float>(graphicWindows->getHeight());
        projWidth = static_cast<float>(graphicWindows->getWidth());
    }

    // Destructor
    Graphics::~Graphics()
    {
        // Clean up all models and unload textures
        for (int i = 0; i < models.size(); i++)
        {
            models[i].cleanup();
        }

        // Clear global maps after cleanup
        Graphics::models.clear();
        Graphics::textures.clear();
        Graphics::meshes.clear();
    }

    // Initialize the system
    void Graphics::Initialize()
    {
        // Register graphics related components 
        ecsInterface.RegisterComponent<TransformComponent>();
        ecsInterface.RegisterComponent<RenderComponent>();
        ecsInterface.RegisterComponent<LayerComponent>();
        ecsInterface.RegisterComponent<TextComponent>();

        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<RenderComponent>()); //Set mEntities to be populated with RenderComponent holding Entities

        ecsInterface.SetSystemSignature<Graphics>(signature);
        std::cout << "Signature for Graphics system is: " << signature << std::endl;
        //  ---- ECS INITIALIZATION END --- 

        // --- Create Instances -----
        // ---- Create Input Handler Instance (Relocate to appropriate system *Graphic only renders*)
        InputHandlerInstance = InputHandler::GetInstance();

        //Initialize entry points to OpenGL functions and extensions
        if (!glfwInit())
        {
            std::cout << "GLFW init has failed - abort program!!!" << std::endl;
        }
    

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        glfwWindowHint(GLFW_RED_BITS, 8); glfwWindowHint(GLFW_GREEN_BITS, 8);
        glfwWindowHint(GLFW_BLUE_BITS, 8); glfwWindowHint(GLFW_ALPHA_BITS, 8);

        //Initialize entry points for glewInit()
        GLenum err = glewInit();
        if (GLEW_OK != err)
        {
            std::cerr << "Unable to initialize GLEW - error: "
                << glewGetErrorString(err) << " abort program" << std::endl;
        }

        //// Initial Setup - Should Run Once
        //IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking Support

        // Scale up all ImGui elements
        io.FontGlobalScale = 1.1f;

        // Additional docking configurations
        io.ConfigDockingAlwaysTabBar = true;
        io.ConfigDockingTransparentPayload = true;

        // Set ImGui Style
        ImGui::StyleColorsDark();
        gameFramebuffer = CreateFramebuffer(static_cast<int>(projWidth), static_cast<int>(projHeight), gameTexture, rbo);

        // Initialize backends
        ImGui_ImplGlfw_InitForOpenGL(graphicWindows->GetWindow(), true);
        ImGui_ImplOpenGL3_Init("#version 450");

        //INIT Font system
        fontSystem.Initialize();
        // IMPORTANT : setting color of background for program
        SetBackgroundColor(255, 255, 255, 255);
        
        // MAKE ONE MESH, USE GLOBALLY [1:1]
        createMesh(vertices, texCoords, color, "sprite", "fire");
    }

    // Update the system
    void Graphics::Update(float deltaTime)
    {
        // In your Graphics or main game class
        Camera camera({ projWidth, projHeight }); // Viewport size

        // Somewhere in your update loop (e.g., in response to input)
        camera.move({ 0.0f, 0.0f }); // Example of moving the camera to the right
        camera.setZoom(1.2f);       // Example of zooming in

        (void)deltaTime;

        // Bind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, gameFramebuffer);
        glViewport(0, 0, static_cast<GLsizei>(projWidth), static_cast<GLsizei>(projHeight));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //  ------ Graphics Rendering Pipeline START -----
        models.clear();
        // -- Sort renderable entities on layer and ID
      //  if (CurrentSize != mEntities.size()) {
            // Populate the sortedEntities vector with elements from the mEntities set
            sortedEntities.assign(mEntities.begin(), mEntities.end());

            /* debug prints
            // Print initial state of entities with their LayerID, SortID, and Entity ID
            std::cout << "Before Sorting:\n";
            for (auto const& entityId : sortedEntities) {
                auto& layerComponent = ecsInterface.GetComponent<LayerComponent>(entityId);
                std::cout << "Entity ID: " << entityId
                    << ", LayerID: " << layerComponent.layerID
                    << ", SortID: " << layerComponent.sortID << "\n";
            }
            */

            // Sort based on LayerID, then SortID, and finally Entity ID
            std::sort(sortedEntities.begin(), sortedEntities.end(), [](Entity a, Entity b) {
                auto& layerA = ecsInterface.GetComponent<LayerComponent>(a);
                auto& layerB = ecsInterface.GetComponent<LayerComponent>(b);

                // First, compare LayerID
                if (layerA.layerID != layerB.layerID) {
                    return layerA.layerID < layerB.layerID;
                }

                // If LayerID is the same, compare SortID
                if (layerA.sortID != layerB.sortID) {
                    return layerA.sortID < layerB.sortID;
                }

                // If both LayerID and SortID are the same, fall back to Entity ID
                return a < b;
                });

            /* debug prints
            // Print sorted state of entities with their LayerID, SortID, and Entity ID
            /*
            std::cout << "\nAfter Sorting:\n";
            for (auto const& entityId : sortedEntities) {
                auto& layerComponent = ecsInterface.GetComponent<LayerComponent>(entityId);
                std::cout << "Entity ID: " << entityId
                    << ", LayerID: " << layerComponent.layerID
                    << ", SortID: " << layerComponent.sortID << "\n";
            }
             */

            // Update CurrentSize to reflect the new size of mEntities
            CurrentSize = static_cast<unsigned int>(mEntities.size());
      //  }

        for (auto const& entityId : sortedEntities) {
            // Get Components needed
            TransformComponent& transformComponent = ecsInterface.GetComponent<TransformComponent>(entityId);
            RenderComponent& renderComponent = ecsInterface.GetComponent<RenderComponent>(entityId);


            if (ecsInterface.HasComponent<RenderComponent>(entityId)) {
                // Sprite rendering
                Graphics::Model& model = getMesh("sprite"); // Use for mesh

                // Loads the texture if it�s not loaded into map
                /*if (textures.find(renderComponent.textureID) == textures.end()) 
                {
                    textures[renderComponent.textureID] = GlobalAssetManager.GetTextureForGraphics(renderComponent.textureID);
                }*/

                model.textureID = textures[renderComponent.textureID]; // Assign loaded texture ID to model

                // TRANSLATE, ROTATE, SCALE
                glm::vec2 translation(transformComponent.position.x, transformComponent.position.y);
                float rotation = transformComponent.rotation;
                glm::vec2 scale(transformComponent.scale.x, transformComponent.scale.y);

                // SET MATRIX
                model.modelMatrix = Graphics::calculate2DTransform(translation, rotation, scale);

                // Set color and alpha 
                model.color = renderComponent.color;
                model.alpha = renderComponent.alpha;

                // Draw the model
                model.draw();
            }
            if (ecsInterface.HasComponent<TextComponent>(entityId)) {
                // Text rendering
                TextComponent& textComponent = ecsInterface.GetComponent<TextComponent>(entityId);

                // Set active font
                fontSystem.SetActiveFont(textComponent.fontName);

                // Calculate position based on TransformComponent + offset
                glm::vec2 textPosition = transformComponent.position + textComponent.offset;

                // Set up an orthographic projection for 2D text rendering
                glm::mat4 projection = glm::ortho(0.0f, 1600.0f, 900.0f, 0.0f); // Example viewport size, adjust as needed
                //Print here
                // 
                // Render the text
                fontSystem.RenderText(
                    textComponent.text,
                    textPosition.x,
                    textPosition.y,
                    static_cast<float>(textComponent.fontSize),
                    textComponent.color,
                    projection
                );
            }

  
        }

        for (auto& model : models) 
        {
            model.draw(); // Call the draw function on each model
        }
       // std::cout << models.size() << std::endl;


        // Creating particles in update here //
        if (engineState.IsPaused()) { // temporaily use IsPaused for toggling particle reaction
            TransformComponent& BoxTransformComponent = ecsInterface.GetComponent<TransformComponent>(0);
            particleSystem.emitterPosition = BoxTransformComponent.position;
            particleSystem.emit(5, glm::vec2(500.f, -300));
            engineState.SetPaused(false);
        }

        particleSystem.update(deltaTime);
        for (const Particle& p : particleSystem.particles)
        {
            if (p.active)
            {
                Graphics::Model& particle = getMesh("sprite");
                //particle.textureID = GlobalAssetManager.GetTextureForGraphics("fire");
                // Loads the texture if it�s not loaded into map
                /*if (textures.find(("./images/armored_pro.png")) == textures.end()) {
                    particle.textureID = GlobalAssetManager.GetTextureForGraphics("fire");
                }
                particle.textureID = textures["fire"];*/
                glm::vec2 scale_char_test(p.size, p.size);
                particle.modelMatrix = Graphics::calculate2DTransform(p.position, 0, scale_char_test);
                particle.alpha = p.life / 5.0f;
                particle.color = p.color;
                particle.draw();
            }
        }

        // Bind the default framebuffer again
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        //// IMGUI BELOW
        // Start a new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create a full-screen dockspace
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        // Add flags to prevent moving and resizing
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // Begin the main dockspace window
        ImGui::Begin("DockSpace Demo", nullptr, windowFlags);
        ImGui::PopStyleVar(2);

        //  ------ Graphics Rendering Pipeline START -----

        // -- Sort renderable entities on layer and ID
        if (CurrentSize != mEntities.size()) 
        {
            sortedEntities.clear();  // Ensure we're not carrying over any data from previous frames
            sortedEntities.assign(mEntities.begin(), mEntities.end());

            std::sort(sortedEntities.begin(), sortedEntities.end(), [](Entity a, Entity b) {
                auto& layerA = ecsInterface.GetComponent<LayerComponent>(a);
                auto& layerB = ecsInterface.GetComponent<LayerComponent>(b);

                if (layerA.layerID != layerB.layerID)
                    return layerA.layerID < layerB.layerID;
                if (layerA.sortID != layerB.sortID)
                    return layerA.sortID < layerB.sortID;

                return a < b;
                });
            CurrentSize = static_cast<unsigned int>(mEntities.size());
        }

        //render 
        for (auto const& entityId : sortedEntities) 
        {
            TransformComponent& transformComponent = ecsInterface.GetComponent<TransformComponent>(entityId);
            RenderComponent& renderComponent = ecsInterface.GetComponent<RenderComponent>(entityId);

            // Ensure modelMatrix is updated only once per frame
            if (renderComponent.renderType == RenderType::Sprite) 
            {
                Graphics::Model& model = getMesh("sprite");
                /*if (textures.find(renderComponent.textureID) == textures.end()) 
                {
                    textures[renderComponent.textureID] = GlobalAssetManager.GetTextureForGraphics(renderComponent.textureID);
                }*/
                model.textureID = textures[renderComponent.textureID];

                glm::vec2 translation(transformComponent.position.x, transformComponent.position.y);
                model.modelMatrix = Graphics::calculate2DTransform(translation, transformComponent.rotation, transformComponent.scale);
                model.color = renderComponent.color;
                model.alpha = renderComponent.alpha;

                model.draw();
            }
            else if (renderComponent.renderType == RenderType::Text) 
            {
                // Text rendering
                TextComponent& textComponent = ecsInterface.GetComponent<TextComponent>(entityId);

                // Set active font
                fontSystem.SetActiveFont(textComponent.fontName);

                // Calculate position based on TransformComponent + offset
                glm::vec2 textPosition = transformComponent.position + textComponent.offset;

                // Set up an orthographic projection for 2D text rendering
                glm::mat4 projection = glm::ortho(0.0f, projWidth, projHeight, 0.0f); // Example viewport size, adjust as needed
                //Print here
                // 
                // Render the text
                fontSystem.RenderText(
                    textComponent.text,
                    textPosition.x,
                    textPosition.y,
                    static_cast<float>(textComponent.fontSize),
                    textComponent.color,
                    projection
                );
            }
        }


        //  ------ Graphics Rendering Pipeline END -----

        // Creating particles in update here //
       // particleSystem.emit(5, glm::vec2(500.0f, -300.0f));
        particleSystem.update(deltaTime);
        for (const Particle& p : particleSystem.particles)
        {
            if (p.active)
            {
                Graphics::Model& particle = getMesh("sprite");
                glm::vec2 scale_char_test(p.size, p.size);
                particle.modelMatrix = Graphics::calculate2DTransform(p.position, 0, scale_char_test);
                particle.alpha = p.life / 5.0f;
                particle.color = p.color;
                particle.draw();
            }
        }

        // Bind the default framebuffer again
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Create a dockspace ID
        ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");

        // Create the dockspace
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // Optionally add a menu bar
        if (ImGui::BeginMenuBar()) 
        {
            if (ImGui::BeginMenu("Unnamed Studio Game Engine")) 
            {
                if (ImGui::MenuItem("Open"))
                { 
                    // Get the file path from the OpenFileDialog
                    filePath = Graphics::OpenFileDialog();

                    // Ensure the file path is not empty before proceeding
                    if (!filePath.empty())
                    {
                        ecsInterface.ClearEntities();
                        //entityAssets.clear();

                        // This function expects a const std::string& as input
                        //GlobalAssetManager.LoadEntities(filePath);
                    }
                }
                if (ImGui::MenuItem("Save"))
                {
                    std::string savePath = SaveFileDialog();
                    if (!savePath.empty())
                    {
                        //GlobalEntityAsset.SerializeEntities(savePath);
                    }
                    else
                    {
                        std::cout << "Save path selected: " << savePath << std::endl;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        // End the dockspace window
        ImGui::End();

        //for edwin
        if (ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::NewLine();

            RenderErrorPopup();

            // Audio
            if (ImGui::CollapsingHeader("Audio"))
            {
                
            }
            ImGui::End();
        }
      
        // "Global variables" For Victor and Joel for IMGUI portion
        static int selectedModelIndex = -1;
        static char meshNameBuffer[128] = "";  // Buffer for the mesh name
        static char imagePathBuffer[256] = "";  // Buffer for the image path
        static Entity selectedEntity = std::numeric_limits<Entity>::max();  // Sets a selected entity to be a non existent entity out of range
        static bool isPropertiesWindowOpen = false;                         // Allows opening and closing of the name editor? might want to remove

        if (ImGui::Begin("Entity", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::CollapsingHeader("Entity Creation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("Create Entity", ImVec2(140, 30)))
                {
                    // Call to ECS Interface to create a new entity
                    Entity newEntity = ecsInterface.CreateEntity();
                    ecsInterface.AddComponent<TransformComponent>(newEntity, TransformComponent{});
                    ecsInterface.AddComponent<RenderComponent>(newEntity, RenderComponent{});
                    ecsInterface.AddComponent<LayerComponent>(newEntity, LayerComponent{});
                    ecsInterface.AddComponent<TextComponent>(newEntity, TextComponent{});
                    std::cout << "Entity created with ID: " << newEntity << std::endl;
                }


                if (ImGui::Button("Delete All Entity", ImVec2(140, 30)))
                {
                    ImGui::OpenPopup("Confirm Clear All"); // Open the confirmation popup
                    // Clear entities list here.
                }

                // Create the popup modal
                if (ImGui::BeginPopupModal("Confirm Clear All", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("Are you sure?");
                    ImGui::Separator();

                    if (ImGui::Button("Yes", ImVec2(120, 0))) // Confirm button
                    {
                        isPropertiesWindowOpen = false;
                        // Clear all objects and reset fields
                        ecsInterface.ClearEntities();
                        models.clear();

                        ImGui::CloseCurrentPopup(); // Close the popup
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel", ImVec2(120, 0))) // Cancel button
                    {
                        ImGui::CloseCurrentPopup(); // Close the popup without clearing
                    }

                    ImGui::EndPopup();
                }
            }


            if (ImGui::Button("Delete Entity", ImVec2(140, 30)))
            {
                ecsInterface.DestroyEntity(selectedEntity);
            }

            ImGui::Begin("Entity List");
            // Display each entity as a selectable item
            for (Entity entity : ecsInterface.GetEntities())
            {
                std::string entityName = ecsInterface.GetEntityName(entity);

                // Use Selectable for each entity and check if it's clicked
                if (ImGui::Selectable(entityName.c_str(), selectedEntity == entity))
                {
                    selectedEntity = entity;  // Set selected entity to this one
                    isPropertiesWindowOpen = true;  // Open properties window
                }
            }
            ImGui::End();

            // Separate panel to edit the selected entity's name
            if (isPropertiesWindowOpen && selectedEntity != std::numeric_limits<Entity>::max())
            {

                if (!ecsInterface.IsEntityValid(selectedEntity))
                {
                    isPropertiesWindowOpen = false;
                    selectedEntity = std::numeric_limits<Entity>::max(); // Deselect the entity
                }
                else
                {
                    TransformComponent& transformComponent = ecsInterface.GetComponent<TransformComponent>(selectedEntity);
                    RenderComponent& renderComponent = ecsInterface.GetComponent<RenderComponent>(selectedEntity);

                    ImGui::Begin("Entity Properties", &isPropertiesWindowOpen);
                    if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        std::string selectedEntityName = ecsInterface.GetEntityName(selectedEntity);
                        char buffer[256];
                        strncpy(buffer, selectedEntityName.c_str(), sizeof(buffer));
                        buffer[sizeof(buffer) - 1] = '\0';

                        // Display an InputText to edit the selected entity's name
                        if (ImGui::InputText("Edit Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            ecsInterface.SetEntityName(selectedEntity, std::string(buffer));  // Update the name of the selected entity
                        }
                    }

                    if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        // Translation
                        ImGui::InputFloat2("Translation", glm::value_ptr(transformComponent.position));

                        // Rotation
                        ImGui::SliderFloat("Rotation", &transformComponent.rotation, 0.0f, 360.0f);

                        // Scale
                        ImGui::InputFloat2("Scale", glm::value_ptr(transformComponent.scale));
                    }

                    // Show combo box for adding components
                    static int selectedComponentIndex = -1;
                    std::vector<std::string> availableComponents;

                    // Populate availableComponents with components the entity doesn’t already have
                    for (int i = 0; i < ecsInterface.GetComponentNames().size(); ++i)
                    {
                        if (!ecsInterface.GetEntitySignature(selectedEntity).test(i))  // Check if the component is not yet added
                        {
                            availableComponents.push_back(ecsInterface.GetComponentNames()[i]);
                        }
                    }

                    //// Show the combo box with available components
                    //if (ImGui::BeginCombo("Choose Component", selectedComponentIndex >= 0 ? availableComponents[selectedComponentIndex].c_str() : "Select Component"))
                    //{
                    //    for (int i = 0; i < availableComponents.size(); ++i)
                    //    {
                    //        bool isSelected = (selectedComponentIndex == i);
                    //        if (ImGui::Selectable(availableComponents[i].c_str(), isSelected))
                    //        {
                    //            selectedComponentIndex = i;
                    //        }
                    //        if (isSelected)
                    //        {
                    //            ImGui::SetItemDefaultFocus();
                    //        }
                    //    }
                    //    ImGui::EndCombo();
                    //}

                    //// Add button to attach the selected component to the entity
                    //if (selectedComponentIndex >= 0 && ImGui::Button("Add Component"))
                    //{
                    //    std::string selectedComponentName = availableComponents[selectedComponentIndex];

                    //    // Add the selected component to the entity
                    //    ecsInterface.AddComponentToEntity(selectedEntity, selectedComponentName);

                    //    // Reset the combo box selection and clear out selectedComponentIndex to update available components
                    //    selectedComponentIndex = -1;
                    //}
                    
                    if (ImGui::CollapsingHeader("Texture Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        // Start the Imgui combo box for texture selection
                        // combo box = dropdown box
                        if (ImGui::BeginCombo("Change Texture", newTextureName.empty() ? "Select Texture" : newTextureName.c_str()))
                        {

                            ImGui::EndCombo();                                          // End combo box
                        }

                        // Apply texture change
                        if (!newTextureName.empty() && ImGui::Button("Apply Texture"))  // Button to apply selected texture
                        {


                            std::cout << "Texture changed to: " << newTextureName << std::endl;
                        }

                        // Color and Alpha (if applicable)
                        ImGui::ColorEdit3("Color", glm::value_ptr(renderComponent.color));
                        ImGui::SliderFloat("Alpha", &renderComponent.alpha, 0.0f, 1.0f);
                    }

                    // Calculate the Y-position to place the button at the bottom
                    float buttonHeight = ImGui::GetFrameHeightWithSpacing();
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - buttonHeight - ImGui::GetStyle().WindowPadding.y);

                    // Close button
                    if (ImGui::Button("Close", ImVec2(180, 30)))
                    {
                        isPropertiesWindowOpen = false;  // Close properties window
                    }

                    ImGui::End();
                }

                // Reset selectedEntity if the properties window is closed
                if (!isPropertiesWindowOpen)
                {
                    selectedEntity = std::numeric_limits<Entity>::max();
                }
            }
        }
        ImGui::End();

        // Begin the DebugSystem ImGui window
        if (ImGui::Begin("DebugSystem", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {

            // Retrieve the DebugSystem instance
            Framework::DebugSystem* debugSystem = Framework::DebugSystem::GetInstance();

            // Get the cached percentage values
            std::unordered_map<std::string, double> systemPercentages = debugSystem->GetSystemUpdateTimes();

            // Prepare data for histogram
            std::vector<float> values;
            std::vector<const char*> labels;

            for (const auto& [systemName, percentage] : systemPercentages) {
                values.push_back(static_cast<float>(percentage)); // Add percentage as float for the histogram
                labels.push_back(systemName.c_str()); // Store the label for each system
            }

            // Display each system's update percentage as text
            for (size_t i = 0; i < values.size(); ++i) {
                ImGui::Text("%s: %.2f%%", labels[i], values[i]);
            }

            // Plot the histogram for system performance percentages
            if (!values.empty()) {
                ImGui::Text("System Performance Histogram");
                ImGui::PlotHistogram("##SystemUsage", values.data(), static_cast<int>(values.size()), 0, nullptr, 0.0f, 100.0f, ImVec2(0, 100));
            }
        }
        // End the DebugSystem ImGui window
        ImGui::End();

        // Open a new window for game state controls (e.g., "Game Controls")
        if (ImGui::Begin("Game Controls", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav))
        {
            // Get the width of the available region
            float windowWidth = ImGui::GetContentRegionAvail().x;

            // Set button size (adjust values as desired)
            ImVec2 buttonSize(50, 30); // Set the width and height of the button

            // Calculate the X position to center the buttons horizontally
            float centerX = (windowWidth - (3 * buttonSize.x + 2 * ImGui::GetStyle().ItemSpacing.x)) * 0.5f;

            // Add spacing on the left to center-align the buttons
            ImGui::SetCursorPosX(centerX);

            // Play Button with custom color
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));       // Light blue
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.5f, 0.9f, 1.0f)); // Medium blue when hovered
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));  // Darker blue when active
            if (ImGui::Button("Play", buttonSize))
            {
                // Set engine state to play mode
                engineState.SetPlay(true);
            }
            ImGui::PopStyleColor(3); // Restore color

            ImGui::SameLine(); // Place the next button on the same line

            // Pause Button with custom color
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));       // Light blue
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.5f, 0.9f, 1.0f)); // Medium blue when hovered
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));  // Darker blue when active
            if (ImGui::Button("Pause", buttonSize))
            {
                // Toggle pause state while in play mode
                if (engineState.IsPlay())
                {
                    engineState.SetPlay(false);
                }
            }
            ImGui::PopStyleColor(3); // Restore color

            ImGui::SameLine(); // Place the next button on the same line

            // Stop Button with custom color
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));       // Red
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f)); // Darker red when hovered
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));  // Even darker red when active
            if (ImGui::Button("Stop", buttonSize))
            {
                // Stop the game by setting play to false
                engineState.SetPlay(false);
                ecsInterface.ClearEntities(); // Clear all entity and load json to reset scene
                //GlobalAssetManager.LoadEntities(filePath); // temporarily load this scene // cant load
               // GlobalAssetManager.LoadEntities.clear();   
            }
            ImGui::PopStyleColor(3); // Restore color

            ImGui::SameLine(); // Place the next button on the same line
            // Debug Button with custom color
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));       // Light blue
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.5f, 0.9f, 1.0f)); // Medium blue when hovered
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));  // Darker blue when active
            if (ImGui::Button("Debug", buttonSize))
            {
                // Toggle debug state while in play mode
                engineState.SetInDebugMode(!engineState.IsInDebugMode());
            }
            ImGui::PopStyleColor(3); // Restore color
        }
        ImGui::End();

        if (ImGui::Begin("Main Viewport")) 
        {
            // Get the available size in the ImGui window
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            float aspectRatio = static_cast<float>(projWidth) / static_cast<float>(projHeight);

            float newWidth = viewportSize.x;
            float newHeight = viewportSize.x / aspectRatio;

            // Adjust width and height to maintain the aspect ratio
            if (newHeight > viewportSize.y) 
            {
                newHeight = viewportSize.y;
                newWidth = viewportSize.y * aspectRatio;
            }

            // Center the image in the available viewport space with black bars
            ImVec2 offset((viewportSize.x - newWidth) * 0.5f, (viewportSize.y - newHeight) * 0.5f);
            ImGui::SetCursorPos(offset);

            // Render the framebuffer texture within the calculated size
            ImGui::Image((ImTextureID)(intptr_t)gameTexture, ImVec2(newWidth, newHeight), ImVec2(0, 1), ImVec2(1, 0));
        }
        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Handle multiple viewports (for multi-window applications)
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);

        // edwin testing
        glfwSetDropCallback(backup_current_context, DropCallback);
    }

    // Particle System //
    
    // Constructor
    ParticleSystem::ParticleSystem(unsigned int maxParticles, const glm::vec2 emitterPosition)
        : maxParticles(maxParticles), emitterPosition(emitterPosition)
    {
        particles.resize(maxParticles);
        for (Particle& p : particles)
        {
            resetParticles(p);	// Initialize each particles
        }
    }

    void ParticleSystem::update(float deltaTime)
    {
        for (Particle& p : particleSystem.particles)
        {
            if (p.active)
            {
                p.position += p.velocity * deltaTime;	// Update position based on velocity
                p.life -= deltaTime;					// Decrease particle life
                if (p.life <= 0.0f)                     // Check Particle Life
                {
                    resetParticles(p);
                    p.active = false;					// Mark particle as inactive
                }
            }
        }
    }

    void ParticleSystem::emit(unsigned int count, const glm::vec2 initialVelocity)
    {
        for (unsigned int i = 0; i < count; i++)
        {
            Particle* p = getInactiveParticle();                    // Get an inactive particle to reuse
            if (p)
            {
                p->position = emitterPosition;                      // All particles to start from the same emit position
                p->velocity = initialVelocity + randomVelocity();   // Add Random Velocity
                p->active = true;                                   // Activate particle
                p->life = 5.0f;                                     // Particle Life
                p->size = 50.0f;                                    // Particle Size
                p->color = glm::vec3(1.0f, 1.0f, 1.0f);             // Particle Color
            }
        }
    }

    void ParticleSystem::resetParticles(Particle& p)
    {
        p.position = emitterPosition;				// Set the Emitter as the starting point
        p.velocity = glm::vec2(0.0, 0.0);			// Reset Velocity
        p.color = glm::vec3(1.0f, 1.0f, 1.0f);		// Color
        p.size = 1.0f;								// Default Size
        p.life = 5.0f;								// Set Life of each Particle
        p.active = false;							// Set active of each Particle
    }

    Particle* ParticleSystem::getInactiveParticle()
    {
        for (Particle& p : particles)				// Iterate through all particles
        {
            if (!p.active)							// Check if active or not
            {
                return &p;							// Return reference to that particle
            }
        }
        return nullptr;			// No available inactive particles
    }

    glm::vec2 ParticleSystem::randomVelocity()
    {
        return glm::vec2							// Return vector of (x,y)
        {
            (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 50,
            (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * 50

        };
    }

    // Get the name of the system
    std::string Graphics::GetName()
    {
        return "Graphics";
    }

    /* @ SETTING BACKGROUND COLOR */
    void Graphics::SetBackgroundColor(int r, int g, int b, GLclampf alpha)
    {
        // Normalize the RGB values from 0-255 to 0-1
        GLclampf normR = r / 255.0f;
        GLclampf normG = g / 255.0f;
        GLclampf normB = b / 255.0f;

        // Set the clear color
        glClearColor(normR, normG, normB, alpha);
    }

    /* @ Model Drawing Function @ */
    // -------------------------- //
    void Graphics::Model::draw()
    {
        //start shader program
        shdr_pgm.Use();
        glBindVertexArray(vaoid);

        // Checking if texture is being used to draw
        bool useTexture = (textureID != 0);

        // Binding the texture if its actually textured
        if (useTexture)
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
        }

        // Set the texture usage flag in the shader
        GLuint useTextureLocation = glGetUniformLocation(shdr_pgm.GetHandle(), "useTexture");
        glUniform1i(useTextureLocation, useTexture);

        // Set the color uniform in the shader
        GLuint colorLocation = glGetUniformLocation(shdr_pgm.GetHandle(), "uColor");
        GLint alphaLocation = glGetUniformLocation(shdr_pgm.GetHandle(), "uAlpha");


        // Setting color and alpha
        glUniform3f(colorLocation, color.r, color.g, color.b);
        glUniform1f(alphaLocation, alpha);

        //for transparent background while loading PNG images
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //clamping of textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Setting matrix transform for Vertex File
        glUniformMatrix4fv(glGetUniformLocation(shdr_pgm.GetHandle(), "modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniformMatrix4fv(glGetUniformLocation(shdr_pgm.GetHandle(), "viewMatrix"), 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniformMatrix4fv(glGetUniformLocation(shdr_pgm.GetHandle(), "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

        //for changing of primitive_type when drawing objects;
        switch (primitive_type)
        {
        case GL_POINTS:
            glPointSize(10.f);
            glDrawArrays(primitive_type, 0, draw_cnt);
            break;

        case GL_LINES:
            glLineWidth(3.f);
            glDrawArrays(primitive_type, 0, draw_cnt);
            glLineWidth(1.f);
            break;

        case GL_TRIANGLE_FAN:
            glDrawArrays(primitive_type, 0, draw_cnt);
            break;

        case GL_TRIANGLES:
            glDrawElements(primitive_type, draw_cnt, GL_UNSIGNED_INT, 0);
            break;
        }

        glBindVertexArray(0);
        shdr_pgm.UnUse();
    }

    // Point Model //
    Graphics::Model Graphics::points_model(const glm::vec2& coordinate, const glm::vec3& clr_vtx)
    {
        // Use the coordinate directly (no conversion to NDC)
        glm::vec2 pos_vtx = coordinate;

        Graphics::Model mdl;

        glCreateBuffers(1, &mdl.vbo_hdl);
        glNamedBufferStorage(mdl.vbo_hdl, sizeof(glm::vec2), &pos_vtx, GL_DYNAMIC_STORAGE_BIT);

        glCreateVertexArrays(1, &mdl.vaoid);
        glEnableVertexArrayAttrib(mdl.vaoid, 0);
        glVertexArrayVertexBuffer(mdl.vaoid, 0, mdl.vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 0, 0);
        glBindVertexArray(0);

        mdl.primitive_type = GL_POINTS;
        //mdl.setup_shdrpgm(UE_vs, UE_fs);
        mdl.color = clr_vtx;
        mdl.draw_cnt = 1; // Only one point
        mdl.primitive_cnt = 1; // Only one point
        mdl.alpha = 1.0f;

        // **Initialize the transformation matrices** (NEW)
        mdl.modelMatrix = glm::mat4(1.0f); // Identity matrix for the model
        mdl.viewMatrix = glm::mat4(1.0f);  // Identity matrix for the view (for 2D)
        mdl.projectionMatrix = glm::ortho(0.0f, projWidth, projHeight, 0.0f);

        return mdl;
    }

    // Lines Model //
    Graphics::Model Graphics::lines_model(const std::pair<glm::vec2, glm::vec2>& line_segment, const glm::vec3& clr_vtx)
    {
        // Use the start and end points of the line segment directly (no conversion to NDC)
        std::vector<glm::vec2> pos_vtx = {
            line_segment.first,   // Start point
            line_segment.second   // End point
        };

        // VBO and VAO setup
        Graphics::Model mdl;

        glCreateBuffers(1, &mdl.vbo_hdl);
        glNamedBufferStorage(mdl.vbo_hdl, sizeof(glm::vec2) * pos_vtx.size(), pos_vtx.data(), GL_DYNAMIC_STORAGE_BIT);


        glCreateVertexArrays(1, &mdl.vaoid);
        glEnableVertexArrayAttrib(mdl.vaoid, 0);
        glVertexArrayVertexBuffer(mdl.vaoid, 0, mdl.vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 0, 0);
        glBindVertexArray(0);

        mdl.primitive_type = GL_LINES;
        //mdl.setup_shdrpgm(UE_vs, UE_fs);
        mdl.draw_cnt = static_cast<GLuint>(pos_vtx.size()); // number of vertices (2 for a line)
        mdl.primitive_cnt = 1; // only one line
        mdl.color = clr_vtx;

        // **Initialize the transformation matrices** (NEW)
        mdl.modelMatrix = glm::mat4(1.0f); // Identity matrix for the model
        mdl.viewMatrix = glm::mat4(1.0f);  // Identity matrix for the view (for 2D)
        mdl.projectionMatrix = glm::ortho(0.0f, 1600.0f, 900.0f, 0.0f); // 1600x900 viewport, non-NDC
        mdl.alpha = 1.0f;

        return mdl;
    }

    // Circle Model //
    Graphics::Model Graphics::trifans_model(float radius, float centerX, float centerY, const glm::vec3& clr_vtx)
    {
        int segments = 100; //Set value to make things smooth

        // Create a vector to hold the vertex positions
        std::vector<glm::vec2> pos_vtx;
        // Start with the center of the circle
        pos_vtx.push_back(glm::vec2(centerX, centerY));

        // Calculate the circle's vertices
        for (int i = 0; i <= segments; ++i)
        {
            float angle = 2.0f * glm::pi<float>() * i / segments; // Angle for each segment
            float x = centerX + radius * cos(angle); // X coordinate
            float y = centerY + radius * sin(angle); // Y coordinate
            pos_vtx.push_back(glm::vec2(x, y)); // Add vertex to vector
        }

        // VBO and VAO setup
        Graphics::Model mdl;

        glCreateBuffers(1, &mdl.vbo_hdl);
        glNamedBufferStorage(mdl.vbo_hdl, sizeof(glm::vec2) * pos_vtx.size(), pos_vtx.data(), GL_DYNAMIC_STORAGE_BIT);


        glCreateVertexArrays(1, &mdl.vaoid);
        glEnableVertexArrayAttrib(mdl.vaoid, 0);
        glVertexArrayVertexBuffer(mdl.vaoid, 0, mdl.vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 0, 0);
        glBindVertexArray(0);

        //mdl.setup_shdrpgm(UE_vs, UE_fs);
        mdl.primitive_type = GL_TRIANGLE_FAN;
        mdl.color = clr_vtx; // Store the color in the model
        mdl.draw_cnt = static_cast<GLuint>(pos_vtx.size());
        mdl.primitive_cnt = mdl.draw_cnt - 2;
        mdl.alpha = 1.0f;

        // **Initialize the transformation matrices** (NEW)
        mdl.modelMatrix = glm::mat4(1.0f); // Identity matrix for the model
        mdl.viewMatrix = glm::mat4(1.0f);  // Identity matrix for the view (for 2D)
        // Set up the orthographic projection for your viewport
        mdl.projectionMatrix = glm::ortho(0.0f, projWidth, projHeight, 0.0f); //1600x900 viewport

        return mdl;
    }

    // Square Model //
    Graphics::Model Graphics::sqaure_model(glm::vec2 position, float width, float height, const glm::vec3& clr_vtx)
    {
        float half_width = width / 2.0f;
        float half_height = height / 2.0f;

        std::vector<glm::vec2> pos_vtx = {
            glm::vec2(position.x - half_width, position.y + half_height), // Top-left
            glm::vec2(position.x + half_width, position.y + half_height), // Top-right
            glm::vec2(position.x + half_width, position.y - half_height), // Bottom-right
            glm::vec2(position.x - half_width, position.y - half_height)  // Bottom-left
        };
        // Indices for drawing two triangles to form the square
        std::vector<GLuint> indices = { 0, 1, 2, 2, 3, 0 };

        // Create VBO and EBO for the vertices and indices


        // Create a VAO
        Graphics::Model mdl;
        glCreateVertexArrays(1, &mdl.vaoid);

        glCreateBuffers(1, &mdl.ebo_hdl);
        glNamedBufferStorage(mdl.ebo_hdl, sizeof(GLuint) * indices.size(), indices.data(), GL_DYNAMIC_STORAGE_BIT);


        glCreateBuffers(1, &mdl.vbo_hdl);
        glNamedBufferStorage(mdl.vbo_hdl, sizeof(glm::vec2) * pos_vtx.size(), pos_vtx.data(), GL_DYNAMIC_STORAGE_BIT);


        // Bind the position buffer
        glEnableVertexArrayAttrib(mdl.vaoid, 0);
        glVertexArrayVertexBuffer(mdl.vaoid, 0, mdl.vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 0, 0);

        // Bind the element buffer for drawing
        glVertexArrayElementBuffer(mdl.vaoid, mdl.ebo_hdl);
        glBindVertexArray(0);

        // Set model properties
        mdl.primitive_type = GL_TRIANGLES;
        //mdl.setup_shdrpgm(UE_vs, UE_fs);
        mdl.color = clr_vtx;  // Use this color in your fragment shader to apply it
        mdl.draw_cnt = static_cast<GLuint>(indices.size());
        mdl.primitive_cnt = mdl.draw_cnt / 3;
        mdl.alpha = 1.0f;

        // **Initialize the transformation matrices** (NEW)
        mdl.modelMatrix = glm::mat4(1.0f); // Identity matrix for the model
        mdl.viewMatrix = glm::mat4(1.0f);  // Identity matrix for the view (for 2D)
        mdl.projectionMatrix = glm::ortho(0.0f, projWidth, projHeight, 0.0f); // 1600x900 viewport, NDC

        return mdl;
    }

    // Create Mesh //
    Graphics::Model Graphics::createMesh(const std::vector<glm::vec2>& vtx_coord,
        const std::vector<glm::vec2>& txt_coords,
        const glm::vec3& clr_vtx,
        const std::string& meshName,
        const std::string& textureName) // Add texture path
    {
        //GLuint textureID = loadTexture(textureFilePath); // Load texture and get its ID
        //GLuint textureID = GlobalAssetManager.GetTextureForGraphics(textureName);
        
        std::vector<GLuint> indices = { 0, 1, 2, 2, 3, 0 };

        // Create buffers and set up VAO (same as before)
        // Create model and VAO
        Graphics::Model mdl;

        glCreateBuffers(1, &mdl.vbo_hdl);
        glNamedBufferStorage(mdl.vbo_hdl, sizeof(glm::vec2) * vtx_coord.size(), vtx_coord.data(), GL_DYNAMIC_STORAGE_BIT);

        glCreateBuffers(1, &mdl.tex_vbo_hdl);
        glNamedBufferStorage(mdl.tex_vbo_hdl, sizeof(glm::vec2) * txt_coords.size(), txt_coords.data(), GL_DYNAMIC_STORAGE_BIT);

        glCreateBuffers(1, &mdl.ebo_hdl);
        glNamedBufferStorage(mdl.ebo_hdl, sizeof(GLuint) * indices.size(), indices.data(), GL_DYNAMIC_STORAGE_BIT);

        glCreateVertexArrays(1, &mdl.vaoid);

        // Vertex positions
        glEnableVertexArrayAttrib(mdl.vaoid, 0);
        glVertexArrayVertexBuffer(mdl.vaoid, 0, mdl.vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 0, 0);

        // Texture coordinates
        glEnableVertexArrayAttrib(mdl.vaoid, 1);
        glVertexArrayVertexBuffer(mdl.vaoid, 1, mdl.tex_vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 1, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 1, 1);

        // Index buffer
        glVertexArrayElementBuffer(mdl.vaoid, mdl.ebo_hdl);
        glBindVertexArray(0);

        // Setup the shader program
        //mdl.setup_shdrpgm(UE_vs, UE_fs);
        mdl.color = clr_vtx;
        //mdl.textureID = textureID; // Store the texture ID in the model
        mdl.primitive_type = GL_TRIANGLES;
        mdl.draw_cnt = static_cast<GLuint>(indices.size());
        mdl.primitive_cnt = mdl.draw_cnt / 3;

        // **Initialize the transformation matrices** (NEW)
        mdl.modelMatrix = glm::mat4(1.0f); // Identity matrix for the model
        mdl.viewMatrix = glm::mat4(1.0f);  // Identity matrix for the view (for 2D)
        mdl.projectionMatrix = glm::ortho(0.0f, projWidth, projHeight, 0.0f); // 1600x900 viewport, NDC

        // Store mesh in the container
        meshes[meshName] = mdl;

        return mdl;
    }

    // Getting Mesh //
    Graphics::Model& Graphics::getMesh(const std::string& name)
    {
        return meshes[name];  // Ensure that the mesh exists
    }

    //  Mesh //
    // Integrated texture coordinate update for animation
    void Graphics::drawMeshWithAnimation(Graphics::Model& mdl, int currFrame, int cols, int rows)
    {
        // Calculate frame-specific texture coordinates
        int col = currFrame % cols;
        int row = currFrame / cols;

        float frameWidth = 1.0f / cols;
        float frameHeight = 1.0f / rows;

        float u_min = col * frameWidth;
        float v_min = row * frameHeight;
        float u_max = u_min + frameWidth;
        float v_max = v_min + frameHeight;

        // Update the texture coordinates dynamically for the frame
        std::vector<glm::vec2> txt_coords = {
            { u_min, v_min },  // Bottom-left
            { u_max, v_min },  // Bottom-right
            { u_max, v_max },  // Top-right
            { u_min, v_max }   // Top-left
        };

        // Update the texture VBO with new texture coordinates
        GLuint tex_vbo_hdl;
        glCreateBuffers(1, &tex_vbo_hdl);
        glNamedBufferStorage(tex_vbo_hdl, sizeof(glm::vec2) * txt_coords.size(), txt_coords.data(), GL_DYNAMIC_STORAGE_BIT);

        glEnableVertexArrayAttrib(mdl.vaoid, 1);
        glVertexArrayVertexBuffer(mdl.vaoid, 1, tex_vbo_hdl, 0, sizeof(glm::vec2));
        glVertexArrayAttribFormat(mdl.vaoid, 1, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(mdl.vaoid, 1, 1);

        // Bind and draw the mesh as usual
        glBindVertexArray(mdl.vaoid);
        glBindVertexArray(0);

        // Cleanup the buffer
        glDeleteBuffers(1, &tex_vbo_hdl);
        mdl.primitive_type = GL_TRIANGLES;
    }


    void Graphics::DrawDebugBox(const glm::vec2& center, float width, float height)
    {
        // Calculate half dimensions
        float halfWidth = width / 2.0f;
        float halfHeight = height / 2.0f;

        // Define the corners of the box
        glm::vec2 topLeft(center.x - halfWidth, center.y + halfHeight);
        glm::vec2 topRight(center.x + halfWidth, center.y + halfHeight);
        glm::vec2 bottomLeft(center.x - halfWidth, center.y - halfHeight);
        glm::vec2 bottomRight(center.x + halfWidth, center.y - halfHeight);

        Graphics::Model& model = getMesh("sprite"); // Use for mesh

        // Loads the texture if it�s not loaded into map
        if (textures.find("Hitbox") == textures.end()) 
        {
            //textures["Hitbox"] = GlobalAssetManager.GetTextureForGraphics("Hitbox");
        }

        model.textureID = textures["Hitbox"]; // Assign loaded texture ID to model

        // TRANSLATE, ROTATE, SCALE
        glm::vec2 translation(center.x, center.y);
        float rotation = 0.f;
        glm::vec2 scale(width,height);

        // SET MATRIX
        model.modelMatrix = Graphics::calculate2DTransform(translation, rotation, scale);

        // Set color and alpha 
        model.color = { 1.0f ,1.0f,1.0f };
        model.alpha = 1.0f;

        // Draw the model
        model.draw();
    }

    // Setting Up Shader Programs //
    void Graphics::Model::setup_shdrpgm(std::string const& vtx_shdr,
        std::string const& frag_shdr)
    {
        //compile shaders
        if (!shdr_pgm.CompileShaderFromString(GL_VERTEX_SHADER, vtx_shdr))
        {
            std::cout << "Vertex shader failed to compile: ";
            std::cout << shdr_pgm.GetLog() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (!shdr_pgm.CompileShaderFromString(GL_FRAGMENT_SHADER, frag_shdr))
        {
            std::cout << "Fragment shader failed to compile: ";
            std::cout << shdr_pgm.GetLog() << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (!shdr_pgm.Link()) //Linking shader 
        {
            std::cout << "Shader program failed to link!" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        if (!shdr_pgm.Validate()) //ensuring that it validates
        {
            std::cout << "Shader program failed to validate!" << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    //For ImGUI 
    GLuint Graphics::CreateFramebuffer(int width, int height, GLuint& texture, GLuint& rbo_fb) 
    {
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

    //FILE OPENING / SAVING
    std::string Graphics::OpenFileDialog()
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Scene Files\0*.scene;*.json;*.xml\0All Files\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            std::string fullPath = std::string(ofn.lpstrFile);

            // Check if the path contains "src/"
            size_t pos = fullPath.find("src/");
            if (pos != std::string::npos) {
                // If found, remove the "src/" part of the path
                std::string relativePath = fullPath.substr(pos + 4);  // Skip "src/" (4 characters)

                // Replace backslashes with forward slashes
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

                // Return the relative path (directly inside the "src" directory)
                return relativePath;
            }
            else {
                // If "src/" is not found, return the full path with backslashes converted
                std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
                return fullPath;
            }
        }
        return "";
    }

    std::string Graphics::SaveFileDialog()
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Scene Files\0*.json;*.scene;*.xml\0All Files\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;

        // Change flag to allow new file creation and prompt for overwrite
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (GetSaveFileNameA(&ofn) == TRUE) 
        {
            std::string fullPath(ofn.lpstrFile);
            std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

            // Ensure .json extension if not already provided
            if (fullPath.substr(fullPath.find_last_of(".") + 1) != "json")
            {
                fullPath += ".json"; // Add .json extension
            }

            return fullPath;
        }
        return "";
    }

    // Error Detection for Audio
    void Graphics::RenderErrorPopup()
    {
        if (showErrorPopup)
        {
            ImGui::OpenPopup("Error");  // Open popup when error flag is set
        }

        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("%s", errorMessage.c_str());  // Display error message
            ImGui::Separator();

            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
                showErrorPopup = false;  // Reset the error flag
            }

            ImGui::EndPopup();
        }
    }

    // Edwin , drag and drop Texture/Audio
    void Graphics::DropCallback(GLFWwindow* window, int count, const char** paths)
    {
        (void)window;
        for (int i = 0; i < count; i++) {
            std::string filePath = paths[i];

            // Get the file extension (everything after the last period)
            std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);  // Convert to lowercase for case-insensitive comparison

            // Check if the file is a texture file (e.g., .png, .jpg, .bmp, etc.)
            if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "bmp" || extension == "tga" || extension == "dds") {
                // This is a texture file, process it as such
                std::string textureName = filePath.substr(filePath.find_last_of("/\\") + 1);  // Get filename
            }
        }
    }

    /* ALL USAGE FUNCTIONS BELOW */
    // ---------------------------- //

    void Graphics::DrawCircle(float radius, float centerX, float centerY, const glm::vec3& clr_vtx)
    {
        Graphics::models.emplace_back(Graphics::trifans_model(radius, centerX, centerY, clr_vtx));
    }

    void Graphics::DrawLine(const std::pair<glm::vec2, glm::vec2>& line_segment, const glm::vec3& clr_vtx)
    {
        Graphics::models.emplace_back(Graphics::lines_model(line_segment, clr_vtx));
    }

    void Graphics::DrawPoint(const glm::vec2& coordinates, const glm::vec3& clr_vtx)
    {
        Graphics::models.emplace_back(Graphics::points_model(coordinates, clr_vtx));
    }

    void Graphics::DrawSquare(glm::vec2 position, float width, float height, const glm::vec3& clr_vtx)
    {
        Graphics::models.emplace_back(Graphics::sqaure_model(position, width, height, clr_vtx));
    }

    glm::mat4 Graphics::calculate2DTransform(const glm::vec2& translation, float rotation, const glm::vec2& scale)
    {
        // Start with an identity matrix
        glm::mat4 model = glm::mat4(1.0f);

        // Apply translation in X and Y
        model = glm::translate(model, glm::vec3(translation, 0.0f));

        // Apply rotation (around the Z-axis)
        model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));

        // Apply scaling in X and Y
        model = glm::scale(model, glm::vec3(scale, 1.0f));

        return model;
    }

    //CAMERA FUNCTIONS
    glm::mat4 Graphics::Camera::getViewMatrix() const
    {
        // Identity matrix
        glm::mat4 view = glm::mat4(1.0f);

        // Step 1: Translate to zoom point (the position around which you want to zoom)
        view = glm::translate(view, glm::vec3(-position, 0.0f));

        // Step 2: Apply zoom (scale)
        view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));

        // Step 3: Translate back by the camera position
        view = glm::translate(view, glm::vec3(position, 0.0f));

        return view;
    }

    void Graphics::Camera::move(const glm::vec2& delta)
    {
        position += delta; // Update camera position
    }

    void Graphics::Camera::setZoom(float newZoom)
    {
        zoom = newZoom;
    }

    void Graphics::Camera::setPosition(const glm::vec2& newPosition)
    {
        position = newPosition;
    }

    // CLEANING AND FREE-ING OF MEMORY //
    void Graphics::Model::cleanup()
    {
        // Delete the VAO
        if (vaoid != 0)
        {
            glDeleteVertexArrays(1, &vaoid);
            vaoid = 0;
        }

        // Delete the VBO for vertex positions
        if (vbo_hdl != 0)
        {
            glDeleteBuffers(1, &vbo_hdl);
            vbo_hdl = 0;
        }

        // Delete the EBO
        if (ebo_hdl != 0)
        {
            glDeleteBuffers(1, &ebo_hdl);
            ebo_hdl = 0;
        }

        // Delete the VBO for texture coordinates
        if (tex_vbo_hdl != 0)
        {
            glDeleteBuffers(1, &tex_vbo_hdl);
            tex_vbo_hdl = 0;
        }

        // Delete the texture (if applicable)
        if (textureID != 0)
        {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }

        // Delete the shader program
        shdr_pgm.DeleteShaderProgram();
    }
}