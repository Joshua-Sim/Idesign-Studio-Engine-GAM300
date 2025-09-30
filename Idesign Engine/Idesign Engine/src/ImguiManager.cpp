
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

extern Framework::Coordinator ecsInterface;

namespace Framework {
    float projWidth{};
    float projHeight{};
    GLuint gameFramebuffer{}, gameTexture{}, rbo{};
    bool print_out = false;
    static std::string filePath{};

    // Error message and flag for popup
    std::string errorMessage;
    bool showErrorPopup = false;

    static TextureProperties g_AudioIcon;
    static bool g_AudioIconLoaded = false;

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
        // .obj files nid to do as well !!!!!!
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
                        texPtr->name = fileName;
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
                        audioPtr->name = fileName;
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
            g_AudioIcon.path = "images/test.png"; // your default icon path
            TextureLoader::loadTexture(g_AudioIcon, false);
            g_AudioIconLoaded = true;
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
                            ecsInterface.ClearEntities();
                            loadScene(filePath);
                        }
                    }
                    if (ImGui::MenuItem("Save"))
                    {
                        std::string savePath = SaveFileDialog();
                        saveScene(savePath);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            ImGui::End();

            //for edwin
            if (ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
            {
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

                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "🛠 Texture Options");
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
                            tex->name = renameBuffers[tex->guid];
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
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (imageSize.x - ImGui::CalcTextSize(tex->name.c_str()).x) * 0.5f);

                    const int maxChars = 10;
                    if (tex->name.length() > maxChars)
                        tex->name = tex->name.substr(0, maxChars - 3) + "...";

                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + imageSize.x);
                    ImGui::TextWrapped("%s", tex->name.c_str());
                    ImGui::PopTextWrapPos();

                    ImGui::EndGroup();
                    ImGui::PopID();

                    count++;
                    if (count % columns != 0)
                        ImGui::SameLine();
                }

                for (const auto& guid : texturesToDelete)
                {
                    Framework::AssetManager::Instance().Unload(guid);
                }

                ImGui::NewLine();
                RenderErrorPopup();
            }
            ImGui::End();

            // Modal state
            static std::string activeEditGuid;
            static char nameBuffer[256] = {};
            static char pathBuffer[256] = {};
            static int currentMode = 0;
            static float volumeBuffer = 1.0f;


            if (ImGui::Begin("Audio Manager"))
            {
                const auto& assetsImgui = AssetManager::Instance().GetAssets();

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

                        ImGui::TableNextRow();

                        // Column 0: Icon
                        ImGui::TableSetColumnIndex(0);
                        if (g_AudioIcon.textureID != 0)
                        {
                            ImGui::Image((ImTextureID)g_AudioIcon.textureID, ImVec2(32, 32));

                            // Right-click context menu on image
                            if (ImGui::BeginPopupContextItem(("ImagePopup_" + audioProps->guid).c_str()))
                            {
                                if (ImGui::MenuItem("Edit"))
                                {
                                    activeEditGuid = audioProps->guid;

                                    strncpy(nameBuffer, audioProps->name.c_str(), sizeof(nameBuffer));
                                    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

                                    strncpy(pathBuffer, audioProps->path.c_str(), sizeof(pathBuffer));
                                    pathBuffer[sizeof(pathBuffer) - 1] = '\0';

                                    volumeBuffer = audioProps->volume;
                                    currentMode = (audioProps->mode == "loop") ? 1 : 0;

                                    ImGui::OpenPopup("EditAudioModal");
                                }

                                if (ImGui::MenuItem("Delete"))
                                {
                                    //AssetManager::Instance().Remove<AudioProperties>(audioProps->guid);
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

                // Edit modal
                if (ImGui::BeginPopupModal("EditAudioModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    auto* audioProps = AssetManager::Instance().Get<AudioProperties>(activeEditGuid);
                    if (audioProps)
                    {
                        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                        {
                            audioProps->name = std::string(nameBuffer);
                        }

                        if (ImGui::InputText("Path", pathBuffer, sizeof(pathBuffer)))
                        {
                            audioProps->path = std::string(pathBuffer);
                        }

                        if (ImGui::SliderFloat("Volume", &volumeBuffer, 0.0f, 1.0f))
                        {
                            audioProps->volume = std::clamp(volumeBuffer, 0.0f, 1.0f);
                            GlobalAudio.UE_SetVolume(audioProps->name, audioProps->volume);
                        }

                        static const char* modes[] = { "oneshot", "loop" };
                        if (ImGui::Combo("Mode", &currentMode, modes, IM_ARRAYSIZE(modes)))
                        {
                            audioProps->mode = modes[currentMode];
                        }

                        if (ImGui::Button("Save")) ImGui::CloseCurrentPopup();
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        ImGui::Text("Audio asset not found.");
                        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }
            ImGui::End();

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
                        // Call to ECS Interface to create a new entity , ideally only transform for clean entity but for now create demo entity
                        Entity newEntity = ecsInterface.CreateEntity();
                        ecsInterface.AddComponent<TransformComponent>(newEntity, TransformComponent{});
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
                            //models.clear();

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
                            if (ecsInterface.HasComponent<TextureProperties>(selectedEntity))
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
                                    ImGui::Image((ImTextureID)currentTexture.textureID, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1, 1));
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
                }
                ImGui::End();
            }

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
}