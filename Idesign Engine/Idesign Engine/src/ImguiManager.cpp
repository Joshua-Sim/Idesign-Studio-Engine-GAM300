
#include "pch.h"
#include <windows.h>
#include <commdlg.h>
#include "Graphics.h"

#include <InputHandler.h>
#include "ComponentList.h"
#include "FontSystem.h"
#include "Coordinator.h"
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "EngineState.h"
#include "Debugger.h"
#include <ImguiManager.h>
#include "SceneManager.h"
#include "NavMeshAgentProperties.h"
#include "NavMeshSystem.h"
#include "Imgui_Hierarchy.h"
#include "NavMeshProperties.h"

extern Framework::Coordinator ecsInterface;

ImVec4 helpYellow = ImVec4(1.0f, 0.9f, 0.4f, 1.0f); // soft golden-yellow
// "Global variables" For Victor and Joel for IMGUI portion
static int selectedModelIndex = -1;
static char meshNameBuffer[128] = "";  // Buffer for the mesh name
static char imagePathBuffer[256] = "";  // Buffer for the image path
static Entity selectedEntity = std::numeric_limits<Entity>::max();  // Sets a selected entity to be a non existent entity out of range
static bool isPropertiesWindowOpen = false;                         // Allows opening and closing of the name editor? might want to remove


namespace Framework {
    float projWidth{};
    float projHeight{};
    GLuint gameFramebuffer{}, gameTexture{}, rbo{};
    bool print_out = false;
    static std::string filePath{};

    // Error message and flag for popup
    std::string errorMessage;
    bool showErrorPopup = false;

    // For Audio
    static TextureProperties g_AudioIcon;
    static bool g_AudioIconLoaded = false;

    static TextureProperties g_ObjIcon;
    static bool g_ObjIconLoaded = false;

    // Get the name of the system
    std::string ImguiManager::GetName()
    {
        return "ImguiManager";
    }
   
    //FILE OPENING / SAVING
    std::string ImguiManager::OpenFileDialog()
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

    std::string ImguiManager::SaveFileDialog()
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
    void ImguiManager::RenderErrorPopup()
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

    GLuint ImguiManager::GetGameFramebuffer()
    {
        return gameFramebuffer;
    }

    // Edwin, drag and drop Texture/Audio/Obj
    void DropCallback(GLFWwindow* window, int count, const char** paths)
    {
        (void)window;

        const std::string textureFolder = "Assets/Textures";
        const std::string audioFolder = "Assets/Audio";
        const std::string meshFolder = "Assets/Objects";

        std::string finalPath;
        std::string error;

        for (int i = 0; i < count; ++i)
        {
            std::string filePath = paths[i];
            std::string extension = filePath.substr(filePath.find_last_of('.') + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

            if (extension == "png" || extension == "jpg" || extension == "jpeg" ||
                extension == "bmp" || extension == "tga" || extension == "dds")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<TextureProperties>(fileName);
                if (existingGuid.empty())
                {
                    std::string finalPath;
                    std::string error;

                    if (MoveFileToAssetFolder(filePath, textureFolder, finalPath, error))
                    {
                        auto texPtr = std::make_unique<TextureProperties>();
                        texPtr->name = std::filesystem::path(fileName).stem().string();
                        texPtr->path = finalPath;
                        TextureLoader::loadTexture(*texPtr, true); // Load using reference
                        AssetManager::Instance().RegisterAsset<TextureProperties>
                            (
                                std::move(texPtr),
                                &g_TexturePropertiesProps,
                                finalPath
                            );
                        AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, "src/Data/TextureAsset.json", "textures");
                        std::cout << "Texture added: " << fileName << " to " << finalPath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move texture:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else
                {
                    std::cout << "Texture already exists: " << fileName << std::endl;
                }
            }

            // Handle audio files
            else if (extension == "mp3" || extension == "wav" || extension == "flac")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<AudioProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, audioFolder, finalPath, error))
                    {
                        auto audioPtr = std::make_unique<AudioProperties>();
                        audioPtr->name = std::filesystem::path(fileName).stem().string();
                        audioPtr->path = finalPath;
                        audioPtr->mode = "loop";
                        audioPtr->soundType = "background";
                        loadAudio(*audioPtr); // Load metadata
                        AssetManager::Instance().RegisterAsset<AudioProperties>(
                            std::move(audioPtr),
                            &g_AudioPropertiesProps,
                            finalPath
                        );
                        AssetManager::Instance().SaveArrayToJson(TypeName<AudioProperties>::value, "src/Data/AudioAsset.json", "AudioProperties");
                        std::cout << "Audio added: " << fileName << " to " << finalPath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move audio:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else
                {
                    std::cout << "Audio already exists: " << fileName << std::endl;
                }
            }

            // Handle obj files
            else if (extension == "obj")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<MeshProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, meshFolder, finalPath, error))
                    {
                        auto meshPtr = std::make_unique<MeshProperties>();
                        meshPtr->name = std::filesystem::path(fileName).stem().string();
                        meshPtr->guid = GenerateGUID(); // Ensure GUID is set
                        meshPtr->path = finalPath;

                        MeshLoader::loadMesh(finalPath, *meshPtr); // Load mesh data

                        AssetManager::Instance().RegisterAsset<MeshProperties>(
                            std::move(meshPtr),
                            &g_MeshProps,
                            finalPath
                        );
                        AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, "src/Data/MeshAsset.json", "mesh");
                        std::cout << "Mesh added: " << fileName << " to " << finalPath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move mesh:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else
                {
                    std::cout << "Mesh already exists: " << fileName << std::endl;
                }
            }
        }
    }

    ImguiManager::ImguiManager() {}
    ImguiManager::~ImguiManager() {

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImguiManager::SetGraphicsSystem(Framework::Graphics* graphicsSystem)
    {
        m_graphicsSystem = graphicsSystem;
    }

    void ImguiManager::Initialize() {
        // This assert will fail if the window pointer is null
        assert(m_graphicsSystem->GetWindow() != nullptr && "ERROR: getWindow() returned nullptr!");
        std::cout << "Imgui Initialized\n";
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

        gameFramebuffer = m_graphicsSystem->CreateFramebuffer(static_cast<int>(1280), static_cast<int>(720), gameTexture, rbo);

        // Initialize backends
        ImGui_ImplGlfw_InitForOpenGL(m_graphicsSystem->GetWindow(), true);
        ImGui_ImplOpenGL3_Init("#version 450");

        if (!g_AudioIconLoaded)
        {
            g_AudioIcon.path = "Assets/Textures/music.png"; // your default icon path
            TextureLoader::loadTexture(g_AudioIcon, false);
            g_AudioIconLoaded = true;
        }

        if (!g_ObjIconLoaded)
        {
            g_ObjIcon.path = "Assets/Textures/obj.png"; // your default icon path
            TextureLoader::loadTexture(g_ObjIcon, false);
            g_ObjIconLoaded = true;
        }
    }

    void ImguiManager::Update(float deltaTime) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        int w, h;
        glfwGetFramebufferSize(m_graphicsSystem->GetWindow(), &w, &h);

        // 3. Only draw your UI if the window has a valid size
        if (w > 0 && h > 0)
        {
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

            ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            RenderMenuBar();

            RenderAssetBar();

            RenderMeshAssets();

            RenderAudioManager();

            RenderEntityMenu();

        }
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
        glfwPollEvents();
        glfwSwapBuffers(m_graphicsSystem->GetWindow());
    }

    void ImguiManager::RenderMenuBar() {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Idesign Engine"))
            {
                if (ImGui::MenuItem("Open"))
                {
                    // Get the file path from the OpenFileDialog
                    filePath = OpenFileDialog();

                    //// Ensure the file path is not empty before proceeding
                    if (!filePath.empty())
                    {
                        // Load non additive
                        Framework::sceneManager.LoadScene(filePath);
                    }
                }
                if (ImGui::MenuItem("Save"))
                {
                    std::string savePath = SaveFileDialog();
                    Framework::sceneManager.SaveScene(savePath);
                }
                if (ImGui::MenuItem("Build Navmesh"))
                {
                    auto navSystem = ecsInterface.GetSystem<NavMeshSystem>();
                    if (navSystem) {
                        navSystem->BuildNavMesh();

                        // Debug: Print some valid navmesh positions
                        std::cout << "\n=== NAVMESH DEBUG INFO ===" << std::endl;

                        // Test some positions to see which are on navmesh
                        std::vector<glm::vec3> testPositions = {
                            glm::vec3(0.0f, 1.0f, 0.0f),
                            glm::vec3(0.0f, 1.0f, 1.5f),
                            glm::vec3(-2.0f, 1.0f, 2.0f),
                            glm::vec3(2.0f, 1.0f, 2.0f),
                            glm::vec3(0.0f, 1.0f, 3.0f),
                            glm::vec3(-3.0f, 1.0f, 2.5f),
                        };

                        std::cout << "Testing positions to find valid navmesh points:" << std::endl;
                        for (const auto& pos : testPositions) {
                            glm::vec3 snapped = navSystem->GetNearestPointOnNavmesh(pos);
                            float distance = glm::distance(pos, snapped);

                            if (distance < 0.5f) { // Close enough = probably on navmesh
                                std::cout << "  ✓ VALID: (" << pos.x << ", " << pos.y << ", " << pos.z
                                    << ") → snapped to (" << snapped.x << ", " << snapped.y << ", " << snapped.z
                                    << ") [dist: " << distance << "]" << std::endl;
                            }
                            else {
                                std::cout << "  ✗ NOT ON NAVMESH: (" << pos.x << ", " << pos.y << ", " << pos.z
                                    << ") → nearest (" << snapped.x << ", " << snapped.y << ", " << snapped.z
                                    << ") [dist: " << distance << "]" << std::endl;
                            }
                        }
                        std::cout << "=========================\n" << std::endl;
                    }
                }

                if (ImGui::MenuItem("Agent Go"))
                {
                    auto navSystem = ecsInterface.GetSystem<NavMeshSystem>();
                    if (navSystem) {
                        const auto& allEntities = ecsInterface.GetEntities();
                        if (allEntities.size() > 1) {
                            Entity agentEntity = allEntities[1]; // Get agent entity

                            // Check if entity has agent component
                            if (!ecsInterface.HasComponent<NavMeshAgentComponent>(agentEntity)) {
                                std::cerr << "Entity " << agentEntity << " is not an agent!" << std::endl;
                                return;
                            }

                            std::cout << "\n=== AGENT PATHFINDING ===" << std::endl;

                            // Get agent's current position
                            auto& transform = ecsInterface.GetComponent<TransformComponent>(agentEntity);
                            glm::vec3 startPos = transform.position;
                            std::cout << "Agent current position: (" << startPos.x << ", "
                                << startPos.y << ", " << startPos.z << ")" << std::endl;

                            // SNAP agent to navmesh
                            glm::vec3 snappedStart = navSystem->GetNearestPointOnNavmesh(startPos);
                            float startDist = glm::distance(startPos, snappedStart);
                            std::cout << "  → Snapped to: (" << snappedStart.x << ", "
                                << snappedStart.y << ", " << snappedStart.z
                                << ") [distance: " << startDist << "]" << std::endl;

                            if (startDist > 5.0f) {
                                std::cerr << "  ⚠ WARNING: Agent too far from navmesh! Placing on navmesh..." << std::endl;
                            }

                            // Update agent position to snapped position
                            transform.position = snappedStart;

                            // SNAP destination to navmesh
                            glm::vec3 destination(-3.0f, 1.0f, 2.5f);
                            std::cout << "Target destination: (" << destination.x << ", "
                                << destination.y << ", " << destination.z << ")" << std::endl;

                            glm::vec3 snappedDest = navSystem->GetNearestPointOnNavmesh(destination);
                            float destDist = glm::distance(destination, snappedDest);
                            std::cout << "  → Snapped to: (" << snappedDest.x << ", "
                                << snappedDest.y << ", " << snappedDest.z
                                << ") [distance: " << destDist << "]" << std::endl;

                            if (destDist > 5.0f) {
                                std::cerr << "  ⚠ WARNING: Destination too far from navmesh! Using nearest point..." << std::endl;
                            }

                            // Set agent destination with snapped positions
                            std::cout << "Finding path..." << std::endl;
                            navSystem->SetAgentDestination(agentEntity, snappedDest);
                            std::cout << "=========================\n" << std::endl;
                        }
                        else {
                            std::cerr << "Not enough entities! Need at least 2 entities." << std::endl;
                        }
                    }
                }

                if (ImGui::MenuItem("Snap Agent to Navmesh"))
                {
                    auto navSystem = ecsInterface.GetSystem<NavMeshSystem>();
                    if (navSystem) {
                        const auto& allEntities = ecsInterface.GetEntities();
                        if (allEntities.size() > 1) {
                            Entity agentEntity = allEntities[1];

                            if (ecsInterface.HasComponent<TransformComponent>(agentEntity)) {
                                auto& transform = ecsInterface.GetComponent<TransformComponent>(agentEntity);
                                glm::vec3 oldPos = transform.position;
                                glm::vec3 newPos = navSystem->GetNearestPointOnNavmesh(oldPos);
                                transform.position = newPos;

                                std::cout << "Agent snapped: (" << oldPos.x << ", " << oldPos.y << ", " << oldPos.z
                                    << ") → (" << newPos.x << ", " << newPos.y << ", " << newPos.z << ")" << std::endl;
                            }
                        }
                    }
                }

                if (ImGui::MenuItem("Debug Navmesh Info"))
                {
                    auto navSystem = ecsInterface.GetSystem<NavMeshSystem>();
                    if (navSystem) {
                        navSystem->PrintNavMeshInfo();
                    }
                }

                if (ImGui::MenuItem("Get Random Navmesh Point"))
                {
                    auto navSystem = ecsInterface.GetSystem<NavMeshSystem>();
                    if (navSystem) {
                        // Try a bunch of random points to find one on the navmesh
                        std::cout << "Searching for random valid navmesh points..." << std::endl;

                        int foundCount = 0;
                        for (int i = 0; i < 20 && foundCount < 5; ++i) {
                            glm::vec3 randomPos(
                                ((rand() % 100) / 100.0f - 0.5f) * 10.0f,  // X: -5 to 5
                                1.0f,                                         // Y: 1.0
                                ((rand() % 100) / 100.0f - 0.5f) * 10.0f   // Z: -5 to 5
                            );

                            glm::vec3 snapped = navSystem->GetNearestPointOnNavmesh(randomPos);
                            float dist = glm::distance(randomPos, snapped);

                            if (dist < 2.0f) {
                                std::cout << "  ✓ Valid point found: (" << snapped.x << ", "
                                    << snapped.y << ", " << snapped.z << ")" << std::endl;
                                foundCount++;
                            }
                        }

                        if (foundCount == 0) {
                            std::cout << "  No valid points found. Navmesh might be very small." << std::endl;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }

    void ImguiManager::RenderMeshAssets() {
        if (ImGui::Begin("Mesh Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(helpYellow, "(❓)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(
                    "- The Mesh panel lists all registered mesh assets (OBJ, FBX, etc.).\n"
                    "- Each mesh uses a placeholder icon — right-click the icon to rename or delete.\n"
                    "- You can undo or redo recent changes using the buttons at the bottom.\n\n"
                    "- Hovering shows mesh details such as VAO/VBO handles, vertex count, and GUID.\n"
                    "- Deleting a mesh unloads it from GPU memory and removes it from the Asset Manager.\n"
                    "\nTip: Keep only necessary meshes loaded — unused geometry increases VRAM usage."
                );
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            float windowWidth = ImGui::GetContentRegionAvail().x;
            float itemWidth = 80.0f;
            int columns = std::max(1, static_cast<int>(windowWidth / itemWidth));

            std::vector<std::string> meshesToDelete;
            const auto& assets = AssetManager::Instance().GetAssets();
            int count = 0;

            static char nameBuffer[256] = {};
            static bool initialized = false;

            // Displaying each asset
            for (const auto& [guid, entry] : assets)
            {
                if (entry.typeName != TypeName<MeshProperties>::value)
                    continue;

                auto* mesh = static_cast<MeshProperties*>(entry.pObject.get());
                if (!mesh || mesh->guid.empty())
                    continue;

                std::string modalId = "EditMeshModal_" + mesh->guid;

                ImGui::PushID(mesh->guid.c_str());
                ImGui::BeginGroup();

                // Centered placeholder image
                float imageSize = 64.0f;
                float imageX = (itemWidth - imageSize) * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + imageX);
                ImGui::Image((ImTextureID)g_ObjIcon.textureID, ImVec2(imageSize, imageSize));

                // Right-click on image
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                {
                    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", mesh->name.c_str());
                    initialized = true;
                    ImGui::OpenPopup(modalId.c_str());
                }

                // Centered mesh name
                const int maxChars = 10;
                std::string displayName = mesh->name;
                if (displayName.length() > maxChars)
                    displayName = displayName.substr(0, maxChars - 3) + "...";

                // Center the truncated name
                float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
                float centerOffset = (itemWidth - textWidth) * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);

                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + itemWidth);
                ImGui::TextWrapped("%s", displayName.c_str());
                ImGui::PopTextWrapPos();

                // Modal popup
                if (ImGui::BeginPopupModal(modalId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Mesh Options");
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    ImGui::Text("Edit Name:");
                    ImGui::InputText("##Name", nameBuffer, IM_ARRAYSIZE(nameBuffer));

                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                    float buttonWidth = 80.0f;
                    float spacing = 10.0f;
                    float totalWidth = buttonWidth * 3 + spacing * 2;
                    float centerX = (ImGui::GetWindowSize().x - totalWidth) * 0.5f;
                    ImGui::SetCursorPosX(centerX);

                    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
                    {
                        std::string newName = nameBuffer;
                        if (newName != mesh->name)
                        {
                            UndoRedoManager::Instance().PushAssetUndo
                            (
                                guid,
                                "MeshProperties", // or TypeName<MeshProperties>::value if you use type traits
                                mesh,
                                *FindPropListByName("MeshProperties") // or use mesh->GetPropList() if available
                            );
                            mesh->name = newName;
                            AssetManager::Instance().MarkDirty(guid);
                        }
                        initialized = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine(0, spacing);
                    if (ImGui::Button("Delete", ImVec2(buttonWidth, 0)))
                    {
                        meshesToDelete.push_back(mesh->guid);
                        initialized = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine(0, spacing);
                    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                    {
                        initialized = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                // Tooltip
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Name: %s", mesh->name.c_str());
                    ImGui::Text("Path: %s", mesh->path.c_str());
                    ImGui::Text("GUID: %s", mesh->guid.c_str());
                    ImGui::Text("VAO: %u", mesh->vao);
                    ImGui::Text("VBO: %u", mesh->vbo);
                    ImGui::Text("EBO: %u", mesh->ebo);
                    ImGui::Text("Index Count: %u", mesh->indexCount);
                    ImGui::EndTooltip();
                }

                ImGui::EndGroup();
                ImGui::PopID();

                count++;
                if (count % columns != 0)
                    ImGui::SameLine();
            }

            // Deferred deletion
            for (const auto& guid : meshesToDelete)
            {
                AssetManager::Instance().Unload(guid);
            }

            //if (ImGui::Button("Undo"))
            //    UndoRedoManager::Instance().UndoAsset();

            //ImGui::SameLine();
            //if (ImGui::Button("Redo"))
            //    UndoRedoManager::Instance().RedoAsset();

            ImGui::NewLine();
            RenderErrorPopup();
            ImGui::End();
        }
    }

    void ImguiManager::RenderAssetBar() {
        //for edwin
        if (ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(helpYellow, "(❓)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(
                    "- The Texture panel displays all loaded texture assets as thumbnails.\n"
                    "- Left-click a texture to view its details; right-click to open edit options.\n"
                    "- Inside the popup you can:\n"
                    "    - Rename the texture (updates its asset entry)\n"
                    "    - Delete it (unloads it from the Asset Manager)\n\n"
                    "- Hover over a texture to see metadata like path, GUID, and resolution.\n"
                    "- Changes are auto-saved via the Asset Manager’s dirty-flag system.\n"
                    "\nTip: Keep texture names short and descriptive (e.g. 'brick_diffuse')."
                );
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            float windowWidth = ImGui::GetContentRegionAvail().x;
            ImVec2 imageSize(64, 64);
            float itemWidth = imageSize.x + 16.0f;
            int columns = std::max(1, static_cast<int>(windowWidth / itemWidth));

            std::vector<std::string> texturesToDelete;

            const auto& assets = AssetManager::Instance().GetAssets();
            int count = 0;

            for (const auto& [guid, entry] : assets)
            {
                if (entry.typeName != TypeName<TextureProperties>::value)
                    continue;

                auto* tex = static_cast<TextureProperties*>(entry.pObject.get());
                if (!tex || tex->textureID == 0 || tex->guid.empty())
                    continue;

                ImTextureID imguiTextureID = (ImTextureID)(intptr_t)tex->textureID;
                std::string modalId = "EditModal_" + tex->guid;

                ImGui::PushID(tex->guid.c_str());
                ImGui::BeginGroup();

                // Display texture
                ImGui::Image(imguiTextureID, imageSize, ImVec2(0, 1), ImVec2(1, 0));

                // Right-click to open modal
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    ImGui::OpenPopup(modalId.c_str());

                // Modal dialog
                if (ImGui::BeginPopupModal(modalId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    static std::unordered_map<std::string, char[256]> renameBuffers;
                    if (renameBuffers.find(tex->guid) == renameBuffers.end())
                    {
                        std::snprintf(renameBuffers[tex->guid], sizeof(renameBuffers[tex->guid]), "%s", tex->name.c_str());
                    }

                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Texture Options");
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("Rename Texture:");
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
                    ImGui::InputText("##Rename", renameBuffers[tex->guid], IM_ARRAYSIZE(renameBuffers[tex->guid]));

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Button row
                    float buttonWidth = 80.0f;
                    float spacing = 10.0f;
                    float totalWidth = buttonWidth * 3 + spacing * 2;
                    float centerX = (ImGui::GetWindowSize().x - totalWidth) * 0.5f;
                    ImGui::SetCursorPosX(centerX);

                    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
                    {
                        std::string newName = renameBuffers[tex->guid];
                        if (newName != tex->name)
                        {
                            tex->name = newName;
                            AssetManager::Instance().MarkDirty(tex->guid);
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine(0, spacing);
                    if (ImGui::Button("Delete", ImVec2(buttonWidth, 0)))
                    {
                        texturesToDelete.push_back(tex->guid);
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine(0, spacing);
                    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                // Tooltip on hover
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Name: %s", tex->name.c_str());
                    ImGui::Text("Path: %s", tex->path.c_str());
                    ImGui::Text("GUID: %s", tex->guid.c_str());
                    ImGui::Text("Size: %d x %d", tex->width, tex->height);
                    ImGui::EndTooltip();
                }

                // Texture name centered below image
                const int maxChars = 10;
                std::string displayName = tex->name;
                if (displayName.length() > maxChars)
                    displayName = displayName.substr(0, maxChars - 3) + "...";

                // Now center based on the truncated name
                float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
                float centerOffset = (imageSize.x - textWidth) * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);

                // Display
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + imageSize.x);
                ImGui::TextWrapped("%s", displayName.c_str());
                ImGui::PopTextWrapPos();

                ImGui::EndGroup();
                ImGui::PopID();

                count++;
                if (count % columns != 0)
                    ImGui::SameLine();
            }

            for (const auto& guid : texturesToDelete)
            {
                AssetManager::Instance().Unload(guid);
            }

            ImGui::NewLine();
            RenderErrorPopup();
            ImGui::End();
        }

    }

    void ImguiManager::RenderAudioManager() {
        if (ImGui::Begin("Audio Manager"))
        {

            ImGui::TextColored(helpYellow, "(❓)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(
                    "- The Audio Manager displays all imported audio clips.\n"
                    "- Right-click the audio icon to rename, change mode (oneshot / loop), or delete.\n"
                    "- You can control playback directly from this panel:\n"
                    "    - 'Play' to test the clip\n"
                    "    - 'Stop' to halt playback\n"
                    "    - Adjust volume with the slider\n\n"
                    "- Green text indicates that an audio clip is currently playing.\n"
                    "- Stopped clips return to idle state automatically when playback ends.\n"
                    "\nTip: Keep sound file names consistent with use-cases, e.g. "
                    "'ui_click', 'engine_loop', or 'explosion_short'.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            const auto& assetsImgui = AssetManager::Instance().GetAssets();

            // Modal state
            static std::string activeEditGuid;
            static char nameBuffer[256] = {};
            static char pathBuffer[256] = {};
            static int currentMode = 0;
            static float volumeBuffer = 1.0f;
            std::vector<std::string> audioToDelete;

            if (ImGui::BeginTable("AudioTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 40);
                ImGui::TableSetupColumn("Details");
                ImGui::TableHeadersRow();

                for (const auto& [handle, entry] : assetsImgui)
                {
                    if (entry.typeName != "AudioProperties") continue;
                    auto* audioProps = static_cast<AudioProperties*>(entry.pObject.get());
                    if (!audioProps) continue;
                    std::string audioModal = "EditModal_" + audioProps->guid;

                    ImGui::TableNextRow();

                    // Column 0: Icon
                    ImGui::TableSetColumnIndex(0);
                    if (g_AudioIcon.textureID != 0)
                    {
                        ImGui::Image((ImTextureID)g_AudioIcon.textureID, ImVec2(32, 32));

                        // Right-click to open modal
                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                            ImGui::OpenPopup(audioModal.c_str());

                        // Modal dialog
                        if (ImGui::BeginPopupModal(audioModal.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                        {
                            static char nameBuffer[256] = {};
                            static float volumeBuffer = 1.0f;
                            static int modeBuffer = 0;
                            static bool initialized = false;

                            if (!initialized)
                            {
                                std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", audioProps->name.c_str());
                                modeBuffer = (audioProps->mode == "loop") ? 1 : 0;
                                initialized = true;
                            }

                            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "🎧 Audio Options");
                            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                            ImGui::Text("Rename Audio:");
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
                            ImGui::InputText("##Rename", nameBuffer, IM_ARRAYSIZE(nameBuffer));

                            ImGui::Spacing();
                            ImGui::Text("Playback Mode:");
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
                            static const char* modes[] = { "oneshot", "loop" };
                            ImGui::Combo("##Mode", &modeBuffer, modes, IM_ARRAYSIZE(modes));

                            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                            // Button row
                            float buttonWidth = 80.0f;
                            float spacing = 10.0f;
                            float totalWidth = buttonWidth * 3 + spacing * 2;
                            float centerX = (ImGui::GetWindowSize().x - totalWidth) * 0.5f;
                            ImGui::SetCursorPosX(centerX);

                            if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
                            {
                                std::string newName = nameBuffer;
                                std::string newMode = modes[modeBuffer];

                                bool changed = false;

                                if (newName != audioProps->name)
                                {
                                    audioProps->name = newName;
                                    changed = true;
                                }

                                if (newMode != audioProps->mode)
                                {
                                    audioProps->mode = newMode;
                                    changed = true;
                                }

                                if (changed)
                                    AssetManager::Instance().MarkDirty(audioProps->guid);

                                initialized = false;
                                ImGui::CloseCurrentPopup();
                            }

                            ImGui::SameLine(0, spacing);
                            if (ImGui::Button("Delete", ImVec2(buttonWidth, 0)))
                            {
                                audioToDelete.push_back(audioProps->guid);
                                initialized = false;
                                ImGui::CloseCurrentPopup();
                            }

                            ImGui::SameLine(0, spacing);
                            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                            {
                                initialized = false;
                                ImGui::CloseCurrentPopup();
                            }

                            ImGui::EndPopup();
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("GUID: %s", audioProps->guid.c_str());
                            ImGui::Text("File: %s", audioProps->path.c_str());
                            ImGui::Text("Volume: %.2f", audioProps->volume);
                            ImGui::Text("Mode: %s", audioProps->mode.c_str());
                            ImGui::EndTooltip();
                        }
                    }

                    // Column 1: Details
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("Name: %s", audioProps->name.c_str());
                    ImGui::Text("Duration: %.2f sec", audioProps->duration);

                    // Play/Stop buttons
                    if (ImGui::Button(("Play##" + audioProps->guid).c_str()))
                    {
                        GlobalAudio.UE_PlaySound(audioProps->name);
                        audioProps->isPlaying = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(("Stop##" + audioProps->guid).c_str()))
                    {
                        GlobalAudio.Reset();
                        audioProps->isPlaying = false;
                        audioProps->channelID = -1;
                    }

                    // Volume slider
                    float volume = audioProps->volume;
                    if (ImGui::SliderFloat(("Volume##" + audioProps->guid).c_str(), &volume, 0.0f, 1.0f))
                    {
                        audioProps->volume = std::clamp(volume, 0.0f, 1.0f);
                        GlobalAudio.UE_SetVolume(audioProps->name, audioProps->volume);
                    }

                    // Playback status
                    if (audioProps->isPlaying)
                    {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Status: Playing");
                    }
                    else
                    {
                        ImGui::Text("Status: Stopped");
                    }

                    // Poll playback status
                    if (audioProps->isPlaying && audioProps->channelID != -1)
                    {
                        if (!GlobalAudio.IsChannelPlaying(audioProps->channelID))
                        {
                            audioProps->isPlaying = false;
                            audioProps->channelID = -1;
                        }
                    }
                }
                ImGui::EndTable();
            }

            for (const auto& guid : audioToDelete)
            {
                AssetManager::Instance().Unload(guid);
            }
            ImGui::End();
        }
    }

    void ImguiManager::RenderEntityMenu() {
        if (ImGui::Begin("Entity", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            // -------------------------------
            // SECTION 1: ENTITY CREATION
            // -------------------------------
            if (ImGui::CollapsingHeader("Create New Entity"))
            {
                // --- Help tooltip icon (right-aligned) ---
                ImGui::SameLine();
                ImGui::TextColored(helpYellow, "(❓)");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted(
                        "How to use Entity Creation:\n\n"
                        "Create Basic Entity - Adds a new empty object with a Transform, Mesh, "
                        "and default texture. Use this for normal scene objects.\n\n"
                        "Create Shape Entity - Lets you choose a debug primitive "
                        "(Box, Sphere, or Triangle) for testing geometry or physics.\n\n"
                        "Tips:\n"
                        "- Adjust Transform in the Properties panel after creation.\n"
                        "- Use the Entity List below to select and rename entities.\n"
                        "- Use 'Delete ALL Entities' to reset the scene safely."
                    );
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }

                ImGui::Text("Choose what type of entity to create:");
                ImGui::Separator();
                ImGui::Spacing();

                // --- Simple Entity ---
                if (ImGui::Button("Create Basic Entity", ImVec2(220, 35)))
                {
                    Entity newEntity = ecsInterface.CreateEntity();

                    // ------ Default components added here ----------
                    ecsInterface.AddComponent<TransformComponent>(newEntity, TransformComponent{}); // Default transform
                    ecsInterface.AddComponent<HierarchyComponent>(newEntity, HierarchyComponent{}); // Default hierarchy

                    auto* tex = AssetManager::Instance().Get<TextureProperties>(
                        AssetManager::Instance().FindGuid<TextureProperties>());

                    if (!tex)
                    {
                        auto textureComponent = std::make_unique<TextureProperties>();
                        textureComponent->path = "Assets/PNG/test.png";
                        TextureLoader::loadTexture(*textureComponent, true);
                        AssetManager::Instance().RegisterAsset(std::move(textureComponent), &g_TexturePropertiesProps, "Assets/PNG/test.png");
                        tex = AssetManager::Instance().Get<TextureProperties>(
                            AssetManager::Instance().FindGuid<TextureProperties>());
                    }

                    if (tex) ecsInterface.AddComponent<TextureProperties>(newEntity, *tex);

                    MeshProperties meshComponent{};
                    MeshLoader::loadMesh(meshComponent.path, meshComponent);
                    meshComponent.debugShape = DebugShapeType::None;
                    ecsInterface.AddComponent<MeshProperties>(newEntity, meshComponent);

                    std::cout << "[ECS] Created Basic Entity ID: " << newEntity << std::endl;
                }

                ImGui::Spacing();

                // --- Shape Entity ---
                static int selectedShapeIndex = 1;
                const char* shapeNames[] = { "None", "Box", "Sphere", "Triangle" };
                ImGui::Combo("Shape Type", &selectedShapeIndex, shapeNames, IM_ARRAYSIZE(shapeNames));

                if (ImGui::Button("Create Shape Entity", ImVec2(220, 35)))
                {
                    Entity newEntity = ecsInterface.CreateEntity();
                    ecsInterface.AddComponent<TransformComponent>(newEntity, TransformComponent{});

                    MeshProperties meshComponent{};
                    MeshLoader::loadMesh(meshComponent.path, meshComponent);
                    meshComponent.debugShape = static_cast<DebugShapeType>(selectedShapeIndex);
                    ecsInterface.AddComponent<MeshProperties>(newEntity, meshComponent);

                    std::cout << "[ECS] Created Shape Entity ID: " << newEntity << std::endl;
                }

                ImGui::Spacing();

                // --- Text Entity ---
                if (ImGui::Button("Create Text Entity", ImVec2(220, 35)))
                {
                    Entity textEntity = ecsInterface.CreateEntity();
                    ecsInterface.AddComponent<TransformComponent>(textEntity, TransformComponent{});
                    ecsInterface.AddComponent<TextProperties>(textEntity, TextProperties{});

                    std::cout << "[ECS] Created Text Entity ID: " << textEntity << std::endl;
                }
            }

            // -------------------------------
            // SECTION 2: ENTITY MANAGEMENT
            // -------------------------------
            if (ImGui::CollapsingHeader("Entity Management")) {
                ImGui::Spacing();
                if (ImGui::Button("Delete Selected Entity", ImVec2(220, 35))) {
                    ImGui::OpenPopup("Confirm Delete Entity?");
                }

                if (ImGui::BeginPopupModal("Confirm Delete Entity?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Are you sure you want to delete this entity?");
                    ImGui::Separator();

                    if (ImGui::Button("Yes", ImVec2(100, 30)))
                    {
                        if (selectedEntity != 0)
                        {
                            ecsInterface.DestroyEntity(selectedEntity);
                            selectedEntity = 0;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(100, 30)))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::Button(" Delete ALL Entities", ImVec2(220, 35)))
                {
                    ImGui::OpenPopup("Confirm Clear All");
                }

                if (ImGui::BeginPopupModal("Confirm Clear All", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("Are you sure you want to clear all entities?");
                    ImGui::Separator();

                    if (ImGui::Button("Yes", ImVec2(100, 30)))
                    {
                        ecsInterface.ClearEntities();
                        selectedEntity = 0;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(100, 30)))
                        ImGui::CloseCurrentPopup();

                    ImGui::EndPopup();
                }
            }


            if (ImGui::Begin("Entity List [Hierarchy]"))
            {
                ImGui::SameLine();
                ImGui::TextColored(helpYellow, "(❓)");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted(
                        "Entity List Instructions:\n\n"
                        "- Click an entity name to select it and open its properties.\n"
                        "- Scene entities are grouped by scene name; ungrouped entities appear at the top.\n"
                        "- Use the 'Delete Selected Entity' button to remove a single item.\n"
                        "- Use 'Delete ALL Entities' (with confirmation) to clear everything.\n"
                        "- You can add more components (e.g., Light, Text, Collider) from the Properties window."
                    );
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }

            // --- Help tooltip icon under Entity List header ---


            ImGui::Spacing();

            // ---- Entity Inspector List ---

            std::unordered_set<Entity> registered;
            for (const auto& [sceneName, entities] : Framework::sceneManager.GetSceneEntities())
                registered.insert(entities.begin(), entities.end());

            // 1. Entities with no scene (not recommended, but handled)
            ImGui::PushID("NonSceneHeader");
            bool nonSceneOpen = ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen);

            // Make non-scene header a drop target
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_GUID"))
                {
                    const char* data = (const char*)payload->Data;
                    std::string draggedGUID(data);

                    // Move entity to non-scene (orphan) status
                    MoveEntityToScene(draggedGUID, ""); // Empty string = no scene
                }
                ImGui::EndDragDropTarget();
            }

            // Visual feedback while dragging
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
                {
                    if (payload->IsDataType("ENTITY_GUID"))
                    {
                        ImVec2 min = ImGui::GetItemRectMin();
                        ImVec2 max = ImGui::GetItemRectMax();
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        drawList->AddRect(min, max, IM_COL32(100, 200, 255, 200), 0.0f, 0, 2.0f);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (nonSceneOpen)
            {
                std::vector<std::string> rootGUIDs;

                for (Entity entity : ecsInterface.GetEntities())
                {
                    if (!registered.count(entity) && ecsInterface.IsEntityValid(entity))
                    {
                        const auto& guid = ecsInterface.GetEntityGUID(entity);
                        const auto& h = ecsInterface.GetComponent<HierarchyComponent>(entity);
                        if (h.parentGUID.empty())
                        {
                            rootGUIDs.push_back(guid);
                        }
                    }
                }

                std::sort(rootGUIDs.begin(), rootGUIDs.end(), [](const std::string& a, const std::string& b) {
                    Entity ea = ecsInterface.GetEntityByGUID(a);
                    Entity eb = ecsInterface.GetEntityByGUID(b);
                    return ecsInterface.GetComponent<HierarchyComponent>(ea).sortIndex <
                        ecsInterface.GetComponent<HierarchyComponent>(eb).sortIndex;
                    });

                for (const auto& guid : rootGUIDs)
                {
                    DrawEntityHierarchy(guid, selectedEntity, isPropertiesWindowOpen);
                }
            }
            ImGui::PopID();

            // 2. Entities grouped by scene, shown with hierarchy
            for (const auto& [sceneName, entities] : Framework::sceneManager.GetSceneEntities())
            {
                std::string sceneLabel = sceneName + " [Scene]";
                std::string sceneHeaderID = "SceneHeader_" + sceneName;

                ImGui::PushID(sceneHeaderID.c_str());

                bool isSceneOpen = ImGui::CollapsingHeader(sceneLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

                // Make scene header a drop target
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_GUID"))
                    {
                        const char* data = (const char*)payload->Data;
                        std::string draggedGUID(data);

                        // Move entity to this scene
                        if (CanMoveToScene(draggedGUID, sceneName))
                        {
                            MoveEntityToScene(draggedGUID, sceneName);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // Visual feedback while dragging over scene header
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
                    {
                        if (payload->IsDataType("ENTITY_GUID"))
                        {
                            const char* data = (const char*)payload->Data;
                            std::string draggedGUID(data);

                            if (CanMoveToScene(draggedGUID, sceneName))
                            {
                                // Highlight the scene header
                                ImVec2 min = ImGui::GetItemRectMin();
                                ImVec2 max = ImGui::GetItemRectMax();
                                ImDrawList* drawList = ImGui::GetWindowDrawList();
                                drawList->AddRect(min, max, IM_COL32(100, 200, 255, 200), 0.0f, 0, 2.0f);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (isSceneOpen)
                {
                    std::vector<std::string> rootGUIDs;

                    for (Entity entity : entities)
                    {
                        if (!ecsInterface.IsEntityValid(entity)) continue;

                        const auto& guid = ecsInterface.GetEntityGUID(entity);
                        const auto& h = ecsInterface.GetComponent<HierarchyComponent>(entity);
                        if (h.parentGUID.empty())
                        {
                            rootGUIDs.push_back(guid);
                        }
                    }

                    std::sort(rootGUIDs.begin(), rootGUIDs.end(), [](const std::string& a, const std::string& b) {
                        Entity ea = ecsInterface.GetEntityByGUID(a);
                        Entity eb = ecsInterface.GetEntityByGUID(b);
                        return ecsInterface.GetComponent<HierarchyComponent>(ea).sortIndex <
                            ecsInterface.GetComponent<HierarchyComponent>(eb).sortIndex;
                        });

                    for (const auto& guid : rootGUIDs)
                    {
                        DrawEntityHierarchy(guid, selectedEntity, isPropertiesWindowOpen);
                    }
                }

                ImGui::PopID();
            }

            // ---- Entity Inspector List END ---

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
                        ImGui::InputFloat3("Translation", glm::value_ptr(transformComponent.position));

                        // Rotation
                        ImGui::InputFloat3("Rotation", glm::value_ptr(transformComponent.rotation));

                        // Scale
                        ImGui::InputFloat3("Scale", glm::value_ptr(transformComponent.scale));
                    }

                    // Show combo box for adding components
                    static int selectedComponentIndex = -1;
                    std::vector<std::string> availableComponents;

                    // Populate availableComponents with components the entity doesn�t already have
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

                    if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (ecsInterface.HasComponent<MeshProperties>(selectedEntity))
                        {
                            MeshProperties& meshComponent = ecsInterface.GetComponent<MeshProperties>(selectedEntity);

                            // Build list of mesh labels and GUIDs
                            std::vector<std::string> meshNames;
                            std::vector<AssetHandle> meshGuids;

                            for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
                            {
                                if (entry.typeName != TypeName<MeshProperties>::value)
                                    continue;

                                auto* mesh = static_cast<MeshProperties*>(entry.pObject.get());
                                std::string label = mesh->path.empty() ? "Unnamed Mesh" : mesh->path;
                                meshNames.push_back(label);
                                meshGuids.push_back(guid);
                            }

                            // Convert to const char* array for ImGui
                            std::vector<const char*> labelPtrs;
                            for (const auto& name : meshNames)
                                labelPtrs.push_back(name.c_str());

                            // Find current mesh index
                            int currentIndex = -1;
                            for (size_t i = 0; i < meshGuids.size(); ++i)
                            {
                                if (meshGuids[i] == meshComponent.guid)
                                {
                                    currentIndex = static_cast<int>(i);
                                    break;
                                }
                            }

                            // Dropdown to select mesh
                            ImGui::Separator();
                            ImGui::Text("Select Mesh:");
                            if (ImGui::Combo("Mesh", &currentIndex, labelPtrs.data(), static_cast<int>(labelPtrs.size())))
                            {
                                auto* selectedMesh = AssetManager::Instance().Get<MeshProperties>(meshGuids[currentIndex]);
                                if (selectedMesh)
                                {
                                    meshComponent = *selectedMesh;
                                }
                            }

                            // Show debug info about mesh
                            ImGui::Text("VAO: %u", meshComponent.vao);
                            ImGui::Text("VBO: %u", meshComponent.vbo);
                            ImGui::Text("EBO: %u", meshComponent.ebo);
                            ImGui::Text("Index Count: %u", meshComponent.indexCount);

                            // Optional: add a reload button
                            if (ImGui::Button("Reload Mesh"))
                            {
                                MeshLoader::loadMesh(meshComponent.path, meshComponent);
                            }

                            ImGui::Separator();
                            if (ImGui::Button("Remove Mesh"))
                            {
                                meshComponent = MeshProperties{}; // Reset to default
                            }
                        }
                        else
                        {
                            ImGui::Text("No MeshComponent attached to this entity.");
                            if (ImGui::Button("Add Mesh Component"))
                            {
                                ecsInterface.AddComponent<MeshProperties>(selectedEntity, MeshProperties{});
                            }
                        }
                    }

                    if (ImGui::CollapsingHeader("Texture Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (ecsInterface.HasComponent<Framework::TextureProperties>(selectedEntity))
                        {
                            auto& currentTexture = ecsInterface.GetComponent<TextureProperties>(selectedEntity);

                            // Build list of texture labels and GUIDs
                            std::vector<std::string> textureNames;
                            std::vector<AssetHandle> textureGuids;

                            for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
                            {
                                if (entry.typeName != TypeName<TextureProperties>::value)
                                    continue;

                                auto* tex = static_cast<TextureProperties*>(entry.pObject.get());
                                std::string label = tex->name.empty() ? tex->path : tex->name;
                                textureNames.push_back(label);
                                textureGuids.push_back(guid);
                            }

                            // Convert to const char* array for ImGui
                            std::vector<const char*> labelPtrs;
                            for (const auto& name : textureNames)
                                labelPtrs.push_back(name.c_str());

                            // Find current texture index
                            int currentIndex = -1;
                            for (size_t i = 0; i < textureGuids.size(); ++i)
                            {
                                if (textureGuids[i] == currentTexture.guid)
                                {
                                    currentIndex = static_cast<int>(i);
                                    break;
                                }
                            }

                            // Dropdown to select from registered textures
                            ImGui::Separator();
                            ImGui::Text("Select Texture:");
                            if (ImGui::Combo("Texture", &currentIndex, labelPtrs.data(), static_cast<int>(labelPtrs.size())))
                            {
                                auto* selectedTexture = AssetManager::Instance().Get<TextureProperties>(textureGuids[currentIndex]);
                                if (selectedTexture)
                                {
                                    currentTexture = *selectedTexture;
                                }
                            }

                            // Display metadata
                            ImGui::Text("GUID: %s", currentTexture.guid.c_str());
                            ImGui::Text("Texture ID: %u", currentTexture.textureID);
                            ImGui::Text("Size: %d x %d", currentTexture.width, currentTexture.height);

                            // Preview current texture
                            if (currentTexture.textureID != 0)
                            {
                                ImGui::Text("Preview:");
                                ImGui::Image((ImTextureID)currentTexture.textureID, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));
                            }

                            ImGui::Separator();
                            if (ImGui::Button("Remove Texture"))
                            {
                                currentTexture = TextureProperties{}; // Reset to default
                            }
                        }
                        else
                        {
                            ImGui::Text("No Texture Component attached to this entity.");
                            if (ImGui::Button("Add Texture Component"))
                            {
                                ecsInterface.AddComponent<TextureProperties>(selectedEntity, TextureProperties{});
                            }
                        }
                    }

                    if (ImGui::CollapsingHeader("Text Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (ecsInterface.HasComponent<TextProperties>(selectedEntity))
                        {
                            auto& textComponent = ecsInterface.GetComponent<TextProperties>(selectedEntity);

                            // Buffer for editing the text string
                            char textBuffer[256];
                            strncpy_s(textBuffer, textComponent.text.c_str(), sizeof(textBuffer));
                            textBuffer[sizeof(textBuffer) - 1] = '\0';

                            if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
                            {
                                textComponent.text = std::string(textBuffer);
                            }

                            // Slider for text scale
                            ImGui::DragFloat("Scale", &textComponent.scale, 0.001f, 0.001f, 10.0f);

                            // Color picker for text color
                            ImGui::ColorEdit3("Color", glm::value_ptr(textComponent.color));
                        }
                        else
                        {
                            ImGui::Text("No TextComponent attached.");
                            if (ImGui::Button("Add Text Component"))
                            {
                                ecsInterface.AddComponent<TextProperties>(selectedEntity, TextProperties{});
                            }
                        }
                    }

                    //Navmesh property
                    if (ImGui::CollapsingHeader("NavMesh Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (ecsInterface.HasComponent<NavMeshComponent>(selectedEntity))
                        {
                            auto& navComp = ecsInterface.GetComponent<NavMeshComponent>(selectedEntity);

                            // Display source mesh selection
                            std::vector<std::string> meshNames;
                            std::vector<AssetHandle> meshGuids;

                            for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
                            {
                                if (entry.typeName != TypeName<MeshProperties>::value)
                                    continue;

                                auto* mesh = static_cast<MeshProperties*>(entry.pObject.get());
                                std::string label = mesh->path.empty() ? "Unnamed Mesh" : mesh->path;
                                meshNames.push_back(label);
                                meshGuids.push_back(guid);
                            }

                            std::vector<const char*> labelPtrs;
                            for (const auto& name : meshNames)
                                labelPtrs.push_back(name.c_str());

                            int currentIndex = -1;
                            for (size_t i = 0; i < meshGuids.size(); ++i)
                            {
                                if (meshGuids[i] == navComp.sourceMeshGUID)
                                {
                                    currentIndex = static_cast<int>(i);
                                    break;
                                }
                            }

                            ImGui::Separator();
                            ImGui::Text("Source Mesh:");
                            if (ImGui::Combo("NavMesh Source", &currentIndex, labelPtrs.data(), static_cast<int>(labelPtrs.size())))
                            {
                                navComp.sourceMeshGUID = meshGuids[currentIndex];
                            }

                            ImGui::Separator();
                            ImGui::Checkbox("Is NavMesh Source", &navComp.isNavMeshSource);
                            ImGui::Checkbox("Runtime Generated", &navComp.runtimeGenerated);

                            ImGui::Separator();
                            ImGui::Text("Recast Parameters:");
                            ImGui::DragFloat("Agent Radius", &navComp.agentRadius, 0.01f, 0.1f, 5.0f);
                            ImGui::DragFloat("Agent Height", &navComp.agentHeight, 0.01f, 0.1f, 5.0f);
                            ImGui::DragFloat("Cell Size", &navComp.cellSize, 0.01f, 0.05f, 1.0f);
                            ImGui::DragFloat("Cell Height", &navComp.cellHeight, 0.01f, 0.05f, 1.0f);
                            ImGui::DragFloat("Max Slope", &navComp.maxSlope, 1.0f, 1.0f, 85.0f);

                            // Optional runtime debug
                            ImGui::Separator();
                            ImGui::Text("Runtime Mesh Data:");
                            ImGui::Text("Vertices: %zu", navComp.positions.size());
                            ImGui::Text("Indices: %zu", navComp.indices.size());

                            if (ImGui::Button("Clear Runtime Buffers"))
                            {
                                navComp.positions.clear();
                                navComp.indices.clear();
                            }

                            if (ImGui::Button("Reset NavMesh Component"))
                            {
                                navComp = NavMeshComponent{};
                            }
                        }
                        else
                        {
                            ImGui::Text("No NavMesh Component attached.");
                            if (ImGui::Button("Add NavMesh Component"))
                            {
                                ecsInterface.AddComponent<NavMeshComponent>(selectedEntity, NavMeshComponent{});
                            }
                        }
                    }


                    // Reset selectedEntity if the properties window is closed
                    if (!isPropertiesWindowOpen)
                    {
                        selectedEntity = std::numeric_limits<Entity>::max();
                    }
                    ImGui::End();
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
                    //ecsInterface.ClearEntities(); // Clear all entity and load json to reset scene
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
                float aspectRatio = 1280.0f / 720.0f; // Use the known FBO dimensions
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
                // jay handle mouse click and get UV coordinates
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImVec2 pos = ImGui::GetIO().MousePos;

                    float u = (pos.x - min.x) / (max.x - min.x);
                    float v = (pos.y - min.y) / (max.y - min.y);
                    v = 1.0f - v; // your Image uses (0,1) to (1,0), so flip

                    // hand normalized UV to Graphics for the readback
                    m_graphicsSystem->RequestPickUV(u, v);

                }
            }
            ImGui::End();
        }
    }
}