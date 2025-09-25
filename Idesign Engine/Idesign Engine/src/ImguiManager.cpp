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
#include "AssetManager.h"
#include "TextureAsset.h"
#include "Debugger.h"
#include <ImguiManager.h>



namespace Framework {
    float projWidth{};
    float projHeight{};
    GLuint gameFramebuffer{}, gameTexture{}, rbo{};
    bool print_out = false;

    // edwin
    auto& textureAssets = GlobalAssetManager.GetAllTextureAssets();
    auto& audioAssets = GlobalAssetManager.GetAllAudioAssets();
    auto& entityAssets = GlobalAssetManager.GetAllEntities();

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

    // Edwin, drag and drop Texture/Audio
    void DropCallback(GLFWwindow* window, int count, const char** paths)
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
                auto it = textureAssets.find(textureName);
                if (it == textureAssets.end())
                {
                    GlobalAssetManager.AddTexture(textureName, filePath);  // Add to texture asset manager
                    std::cout << "Texture added: " << textureName << " from path: " << filePath << std::endl;
                }
                else
                {
                    std::cout << "Texture already exists: " << textureName << std::endl;
                }
            }
            // Check if the file is an audio file (e.g., .mp3, .wav, .ogg, etc.)
            else if (extension == "mp3" || extension == "wav" || extension == "flac")
            {
                // This is an audio file, process it as such
                std::string audioName = filePath.substr(filePath.find_last_of("/\\") + 1);  // Get filename
                auto it = audioAssets.find(audioName);
                if (it == audioAssets.end())
                {
                    GlobalAssetManager.AddAudio(audioName, filePath);  // Add to audio asset manager
                    std::cout << "Audio file added: " << audioName << " from path: " << filePath << std::endl;
                }
                else {
                    std::cout << "Audio file already exists: " << audioName << std::endl;
                }
            }
            else if (extension == "ogg")
            {
                // Unsupported .ogg file type - set error message and trigger popup
                errorMessage = "Unsupported file type: ." + extension + "\nPlease use supported formats (.mp3, .wav, .flac).";
                showErrorPopup = true;
            }
            else
            {
                // Handle other file types (non-texture and non-audio files)
                std::cout << "Unsupported file type: " << extension << " for file: " << filePath << std::endl;
            }
        }
    }

    // Declare the map for storing dropped texture files
    std::map<std::string, TextureAsset::Texture> droppedTextureFiles;
    std::vector<std::pair<std::string, std::string>> updates;

    ImguiManager::ImguiManager() {}
    ImguiManager::~ImguiManager() {}

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

    }

    void ImguiManager::Update(float deltaTime) {
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

        ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Unnamed Studio Game Engine"))
            {
                if (ImGui::MenuItem("Open"))
                {
                    // Get the file path from the OpenFileDialog
                    filePath = OpenFileDialog();

                    // Ensure the file path is not empty before proceeding
                    if (!filePath.empty())
                    {
                        ecsInterface.ClearEntities();
                        entityAssets.clear();

                        // This function expects a const std::string& as input
                        GlobalAssetManager.LoadEntities(filePath);
                    }
                }
                if (ImGui::MenuItem("Save"))
                {
                    std::string savePath = SaveFileDialog();
                    if (!savePath.empty())
                    {
                        GlobalEntityAsset.SerializeEntities(savePath);
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
        ImGui::End();

        //for edwin
        if (ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::CollapsingHeader("Sprites", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int columns = 3; // Adjust this to the number of textures you want per row
                int count = 0;
                static char newNameBuffer[256] = { 0 }; // Buffer for name editing

                std::vector<std::string> texturesToDelete;  // Vector to hold textures marked for deletion

                for (auto& texturePair : textureAssets)  // textureAssets is loaded at line 60 of Graphics.cpp
                {
                    const std::string& assetName = texturePair.first;  // Name of texture
                    auto& textureAsset = texturePair.second;
                    GLuint textureID = GlobalAssetManager.GetTextureForGraphics(assetName);  // Get the textureID of the asset

                    ImTextureID imguiTextureID = static_cast<ImTextureID>(textureID);  // Cast to ImTextureID to display in ImGui
                    ImVec2 imageSize(64, 64);  // Choose imageSize (x, y)
                    ImGui::Image(imguiTextureID, imageSize);  // Create Image to display

                    ImGui::SameLine();

                    // If the selected texture matches the current asset name
                    if (selectedTextureName == assetName)
                    {
                        if (ImGui::InputText(("##edit" + selectedTextureName).c_str(), newNameBuffer, sizeof(newNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            if (strlen(newNameBuffer) > 0)
                            {
                                std::string newName = newNameBuffer;  // Get the new name entered by the user
                                std::cout << "New Name is : " << newName << std::endl;

                                // Collect the name change
                                updates.push_back({ assetName, newName });

                                // Update local variables to reflect the new name
                                selectedTextureName = newName;  // Update selection to the new name
                                std::cout << "Updated local variable is : " << selectedTextureName << std::endl;

                                // Update buffer to reflect the new name
                                strncpy(newNameBuffer, newName.c_str(), sizeof(newNameBuffer));

                                // Optionally reset selection after update
                                selectedTextureName = ""; // Reset the selection after update

                                newTextureName = newName;
                            }
                        }
                    }
                    else
                    {
                        if (ImGui::Selectable(assetName.c_str(), selectedTextureName == assetName))  // Select to rename
                        {
                            selectedTextureName = assetName;
                            strncpy(newNameBuffer, assetName.c_str(), sizeof(newNameBuffer));  // Initialize buffer with current name
                        }
                    }

                    // Right-click to delete
                    if (ImGui::IsItemClicked(1))  // 1 corresponds to right-click in ImGui
                    {
                        texturesToDelete.push_back(assetName);  // Mark texture for deletion
                    }

                    // Adjust layout to place textures in rows
                    count++;
                    if (count % columns != 0)
                        ImGui::SameLine();
                    else
                        ImGui::NewLine();
                }

                // Apply the deletions
                for (const auto& textureName : texturesToDelete)
                {
                    GlobalAssetManager.DeleteTexture(textureName);  // Delete the texture from the asset manager
                    std::cout << "Deleted texture: " << textureName << std::endl;
                }
                texturesToDelete.clear();

                // Now, outside of the loop, apply the updates
                for (const auto& update : updates)
                {
                    // Safely update the texture name and reload it
                    GlobalAssetManager.UpdateTextureName(update.first, update.second);
                    std::cout << "Updated texture: " << update.first << " to " << update.second << std::endl;
                }
                updates.clear();
            }
            ImGui::NewLine();

            RenderErrorPopup();

            // Audio
            if (ImGui::CollapsingHeader("Audio"))
            {
                for (const auto& audioPair : audioAssets)
                {
                    const std::string& assetName = audioPair.first;

                    // Check if this audio asset is being edited
                    bool& isEditingThisName = editingStates[assetName];

                    if (!isEditingThisName)
                    {
                        // If not editing, display the name normally and allow selection
                        if (ImGui::Selectable((assetName + "##Selectable").c_str(), selectedAudioName == assetName))
                        {
                            selectedAudioName = assetName;  // Set the selected name
                        }

                        // Double-click to enter editing mode
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        {
                            isEditingThisName = true; // Enter editing mode
                        }
                    }
                    else
                    {
                        strncpy(newAudioNameBuffer, selectedAudioName.c_str(), sizeof(newAudioNameBuffer) - 1); // Initialize the buffer with the current name
                        newAudioNameBuffer[sizeof(newAudioNameBuffer) - 1] = '\0'; // Ensure null termination

                        // Create an input field to edit the audio name
                        if (ImGui::InputText("##RenameAudio", newAudioNameBuffer, sizeof(newAudioNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            std::string newAudioName(newAudioNameBuffer);
                            if (!newAudioName.empty() && newAudioName != selectedAudioName)
                            {
                                // Call the AssetManager function to rename the audio
                                GlobalAssetManager.UpdateAudioName(selectedAudioName, newAudioName);
                                selectedAudioName = newAudioName; // Update the selected name
                            }
                            // Exit editing mode after renaming
                            isEditingThisName = false;
                        }

                        // Optionally, allow the user to click outside and exit editing mode
                        if (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())
                        {
                            isEditingThisName = false; // Exit editing mode
                        }
                    }
                    // Handle deleting the audio asset using a keyboard shortcut (Delete key)
                    if (selectedAudioName == assetName && ImGui::IsKeyPressed(ImGuiKey_Delete))
                    {
                        ImGui::OpenPopup(("Confirm Delete?##" + assetName).c_str());
                    }

                    // Optionally show a confirmation popup for deletion (if desired)
                    if (ImGui::BeginPopupModal(("Confirm Delete?##" + assetName).c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        ImGui::Text("Are you sure you want to delete this audio asset?");
                        if (ImGui::Button("Yes"))
                        {
                            assetsToDelete.push_back(assetName); // Mark for deletion
                            ImGui::CloseCurrentPopup();                 // Close the popup
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("No"))
                        {
                            ImGui::CloseCurrentPopup();                 // Close the popup without deleting
                        }
                        ImGui::EndPopup();
                    }
                }
                // Perform deletions after the loop to avoid iterator invalidation
                for (const auto& name : assetsToDelete)
                {
                    GlobalAssetManager.DeleteAudio(name);
                    selectedAudioName.clear(); // Reset selection if necessary
                }
                assetsToDelete.clear();
            }
            else
            {
                // Reset all editing states when the node is collapsed
                editingStates.clear();
                selectedAudioName.clear();
            }

            static std::string previousSelectedAudioName;

            if (!selectedAudioName.empty())
            {
                // Check if the audio selection has changed
                if (previousSelectedAudioName != selectedAudioName)
                {
                    // Stop or reset the previous audio
                    if (!previousSelectedAudioName.empty())
                    {
                        GlobalAudio.Reset();
                    }

                    // Update the previous selection
                    previousSelectedAudioName = selectedAudioName;
                }

                // Play button
                if (ImGui::Button("Play"))
                {
                    GlobalAudio.UE_PlaySound(selectedAudioName);
                }

                // Pause button
                ImGui::SameLine(); // Place "Pause" button next to "Play" button
                if (ImGui::Button("Pause/Resume"))
                {
                    GlobalAudio.UE_PauseSound(selectedAudioName);
                }

                // Reset button
                ImGui::SameLine();  // Reset button next to pause
                if (ImGui::Button("Reset"))
                {
                    GlobalAudio.Reset();
                }

                // Dropdown Box Mode
                ImGui::Text("Select Mode Option: ");

                // Retrieve the AudioAsset using the AssetManager
                auto audioAsset = GlobalAssetManager.GetAudioAsset(selectedAudioName); // This returns a pointer to the AudioAsset

                static int selectedOption1 = 0; // This will hold the selected index
                std::vector<std::string> modeOptions;

                static int selectedSoundTypeIndex = 0; // This will hold the selected index
                std::vector<std::string> soundTypeOptions;

                if (audioAsset)  // Check if audioAsset is valid
                {
                    try
                    {
                        // Get the current mode for the selected audio asset
                        std::string currentMode = audioAsset->UE_GetMusicMode(selectedAudioName);

                        // Clear previous options to prevent duplication
                        modeOptions.clear();

                        // Add the current mode as the first option and the other mode as the second
                        if (currentMode == "oneshot")
                        {
                            modeOptions.push_back("oneshot");  // Current mode
                            modeOptions.push_back("loop");     // Other mode
                        }
                        else if (currentMode == "loop")
                        {
                            modeOptions.push_back("loop");     // Current mode
                            modeOptions.push_back("oneshot");  // Other mode
                        }
                        else
                        {
                            modeOptions.push_back("Unknown");  // Placeholder for an unknown mode
                            modeOptions.push_back("oneshot");
                            modeOptions.push_back("loop");
                        }

                        // Use ImGui::Combo to allow the user to select an option
                        static std::string selectedMode = currentMode;  // Initialize with the current mode

                        if (selectedMode != currentMode)
                        {
                            selectedMode = currentMode;  // Sync selectedMode with currentMode
                        }

                        if (ImGui::BeginCombo("##Mode", selectedMode.c_str()))
                        {
                            for (const auto& mode : modeOptions)
                            {
                                bool isSelected = (selectedMode == mode);  // Check if the mode matches the selected mode
                                if (ImGui::Selectable(mode.c_str(), isSelected))
                                {
                                    selectedMode = mode;  // Update the selected mode with the string value
                                    modeChanged = true;    // Mark that the mode has changed
                                }
                            }
                            ImGui::EndCombo();
                        }

                        // Handle the mode change
                        if (modeChanged)
                        {
                            std::string newMode = selectedMode;

                            if (newMode != currentMode)
                            {
                                auto audioMap = audioAssets.find(selectedAudioName);
                                if (audioMap != audioAssets.end())
                                {
                                    auto& audioAsset = audioMap->second;

                                    AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(selectedAudioName);

                                    if (musicAsset != nullptr)
                                    {
                                        // Update the mode
                                        musicAsset->mode = newMode;
                                        std::cout << "Updated " << selectedAudioName << " mode to " << newMode << std::endl;

                                        // Serialize the updated audio asset
                                        audioAsset->SerializeAudio("src/Data/AudioAsset.json");
                                        std::cout << "Audio asset serialized after mode update." << std::endl;

                                        // Reset flags after updating
                                        modeChanged = false;
                                    }
                                    else
                                    {
                                        std::cerr << "MusicAsset not found for audio name: " << selectedAudioName << std::endl;
                                    }
                                }
                            }
                            GlobalAudio.Reset();
                        }
                    }
                    catch (const std::runtime_error& e)
                    {
                        // Handle error if the mode retrieval fails
                        ImGui::Text("Error: %s", e.what());
                    }
                }
                else
                {
                    ImGui::Text("Error: AudioAsset not found.");
                }

                // Dropdown Box Sound Type
                ImGui::Text("Select Sound Type: ");

                if (audioAsset)  // Check if audioAsset is valid
                {
                    try
                    {
                        // Get the current SoundType for the selected audio asset
                        Framework::Audio::SoundType soundType = audioAsset->UE_GetMusicSoundType(selectedAudioName);

                        // Clear previous options to prevent duplication
                        soundTypeOptions.clear();

                        // Add available sound types to the options list
                        soundTypeOptions.push_back("Background_Music");
                        soundTypeOptions.push_back("Sound_Effect");
                        soundTypeOptions.push_back("Empty");

                        // Initialize selectedSoundTypeIndex with the index of the current sound type
                        if (soundType == Framework::Audio::SoundType::BACKGROUND_MUSIC)
                        {
                            selectedSoundTypeIndex = 0;
                        }
                        else if (soundType == Framework::Audio::SoundType::SOUND_EFFECT)
                        {
                            selectedSoundTypeIndex = 1;
                        }
                        else if (soundType == Framework::Audio::SoundType::EMPTY)
                        {
                            selectedSoundTypeIndex = 2;
                        }

                        // Create the combo box to allow the user to select a sound type
                        if (ImGui::BeginCombo("##SoundType", soundTypeOptions[selectedSoundTypeIndex].c_str()))
                        {
                            for (int i = 0; i < soundTypeOptions.size(); ++i)
                            {
                                bool isSelected = (selectedSoundTypeIndex == i);
                                if (ImGui::Selectable(soundTypeOptions[i].c_str(), isSelected))
                                {
                                    selectedSoundTypeIndex = i;  // Update the selected sound type
                                    soundTypeChanged = true;     // Mark that the sound type has changed
                                }
                            }
                            ImGui::EndCombo();
                        }

                        // Handle the sound type change
                        if (soundTypeChanged)
                        {
                            Framework::Audio::SoundType newSoundType;
                            switch (selectedSoundTypeIndex)
                            {
                            case 0: newSoundType = Framework::Audio::SoundType::BACKGROUND_MUSIC; break;
                            case 1: newSoundType = Framework::Audio::SoundType::SOUND_EFFECT; break;
                            case 2: newSoundType = Framework::Audio::SoundType::EMPTY; break;
                            default: return;  // Invalid index, do nothing
                            }

                            // Update the sound type in the music asset
                            auto audioMap = audioAssets.find(selectedAudioName);
                            if (audioMap != audioAssets.end())
                            {
                                auto& audioAsset = audioMap->second;

                                AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(selectedAudioName);

                                if (musicAsset != nullptr)
                                {
                                    // Update the sound type
                                    musicAsset->soundType = newSoundType;
                                    std::cout << "Updated " << selectedAudioName << " sound type to " << soundTypeOptions[selectedSoundTypeIndex] << std::endl;

                                    // Serialize the updated audio asset
                                    audioAsset->SerializeAudio("src/Data/AudioAsset.json");

                                    std::cout << "Audio asset serialized after sound type update." << std::endl;

                                    // Reset flags after updating
                                    soundTypeChanged = false;
                                }
                                else
                                {
                                    std::cerr << "MusicAsset not found for audio name: " << selectedAudioName << std::endl;
                                }
                            }
                        }
                    }
                    catch (const std::runtime_error& e)
                    {
                        // Handle error if the sound type retrieval fails
                        ImGui::Text("Error: %s", e.what());
                    }
                }
                else
                {
                    ImGui::Text("Error: AudioAsset not found.");
                }

                // Dropdown Box for Audio Path Selection (BGM or SoundEffect)
                ImGui::Text("Select Audio Path: ");

                if (audioAsset)  // Check if audioAsset is valid
                {
                    try
                    {
                        // Get the current audio path for the selected audio asset inside the try block
                        std::string currentPath = audioAsset->UE_GetMusicFilePath(selectedAudioName);

                        // Remove the file extension from the current path (to get just the folder path)
                        size_t lastSlash = currentPath.find_last_of("/\\"); // Find the last slash in the path
                        std::string folderPath = (lastSlash != std::string::npos) ? currentPath.substr(0, lastSlash + 1) : currentPath;

                        // Extract the file extension from the current path (full file path)
                        size_t dotPos = currentPath.find_last_of(".");
                        std::string fileExtension = (dotPos != std::string::npos) ? currentPath.substr(dotPos + 1) : "";

                        // Check which path is currently selected and set it
                        for (int i = 0; i < audioPathOptions.size(); ++i)
                        {
                            if (audioPathOptions[i] == folderPath)
                            {
                                selectedAudioPathIndex = i;  // Set to the current path index
                                break;
                            }
                        }

                        // Create the combo box for selecting the audio path
                        if (!audioPathOptions.empty())
                        {
                            // Convert audioPathOptions to const char* array for ImGui
                            std::vector<const char*> audioPathCStrOptions;
                            for (const auto& path : audioPathOptions)
                            {
                                audioPathCStrOptions.push_back(path.c_str());
                            }

                            // Display the Combo Box for selecting the audio path
                            if (ImGui::Combo("##AudioPath", &selectedAudioPathIndex, audioPathCStrOptions.data(), audioPathCStrOptions.size()))
                            {
                                // Get the new path selected by the user
                                std::string newPath = audioPathOptions[selectedAudioPathIndex];

                                // Handle the path change only if the new path is different from the current one
                                if (newPath != folderPath)
                                {
                                    // Build the new and old full paths
                                    std::string newFullPath = newPath + selectedAudioName + "." + fileExtension;
                                    std::string oldFullPath = folderPath + selectedAudioName + "." + fileExtension;

                                    // Copy the file to the new folder
                                    if (GlobalAssetManager.CopyAudioToFolder(oldFullPath, newPath))
                                    {
                                        // Delete the original file
                                        if (!GlobalAssetManager.DeleteAudioFile(oldFullPath))
                                        {
                                            std::cerr << "Warning: Failed to delete the original file after copying." << std::endl;
                                        }

                                        // Update the file path in the asset
                                        auto audioMap = audioAssets.find(selectedAudioName);
                                        if (audioMap != audioAssets.end())
                                        {
                                            auto& audioAsset = audioMap->second;
                                            AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(selectedAudioName);

                                            if (musicAsset != nullptr)
                                            {
                                                musicAsset->filePath = newFullPath; // Update with the new full path
                                                std::cout << "Updated " << selectedAudioName << " file path to " << musicAsset->filePath << std::endl;

                                                // Serialize the updated audio asset
                                                audioAsset->SerializeAudio("src/Data/AudioAsset.json");
                                                std::cout << "Audio asset serialized after file path update." << std::endl;
                                            }
                                            else
                                            {
                                                std::cerr << "MusicAsset not found for audio name: " << selectedAudioName << std::endl;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        std::cerr << "Error: Failed to copy the audio file to the new location." << std::endl;
                                    }
                                }
                            }
                        }
                        else
                        {
                            ImGui::Text("No audio paths available.");
                        }
                    }
                    catch (const std::runtime_error& e)
                    {
                        // Handle error if the audio path retrieval fails
                        ImGui::Text("Error: %s", e.what());
                    }
                }
                else
                {
                    ImGui::Text("Error: AudioAsset not found.");
                }
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

                    if (ImGui::CollapsingHeader("Texture Component", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        // Start the Imgui combo box for texture selection
                        // combo box = dropdown box
                        if (ImGui::BeginCombo("Change Texture", newTextureName.empty() ? "Select Texture" : newTextureName.c_str()))
                        {
                            for (auto& texturePair : textureAssets)
                            {
                                const std::string& assetName = texturePair.first;       // Get the name of the texture
                                bool isSelected = (newTextureName == assetName);        // Check if the texture is selected

                                if (ImGui::Selectable(assetName.c_str(), isSelected))   // If the texture is selected in the combo box, set it as the new texture
                                {
                                    newTextureName = assetName;                         // Update the selected texture name
                                }
                                if (isSelected)
                                {
                                    ImGui::SetItemDefaultFocus();           // Auto scroll to and focus on the selected item in the combo box
                                }
                            }
                            ImGui::EndCombo();                                          // End combo box
                        }

                        // Apply texture change
                        if (!newTextureName.empty() && ImGui::Button("Apply Texture"))  // Button to apply selected texture
                        {
                            // Check if the texture name has actually changed
                            std::string currentTexturePath = renderComponent.textureID;

                            auto it = textureAssets.find(newTextureName);               // Find the selected texture in assetmanager
                            if (it == textureAssets.end())
                            {
                                std::cerr << "Texture not found in asset manager: " << newTextureName << std::endl;
                                return;
                            }

                            std::string newTexturePath = it->second->GetPath(newTextureName);   // Retrieve the filepath of the new texture

                            // Assign the new texture name to the render component
                            renderComponent.textureID = newTextureName;

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
                GlobalAssetManager.LoadEntities(filePath); // temporarily load this scene // cant load
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

