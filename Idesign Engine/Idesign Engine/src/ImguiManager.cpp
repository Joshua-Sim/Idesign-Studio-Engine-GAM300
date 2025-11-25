
#include "pch.h"
#include <windows.h>
#include <commdlg.h>
#include "Graphics.h"

#include <InputHandler.h>
#include "FontSystem.h"
#include "Coordinator.h"
#include <algorithm>
#include "AnimationLoader.h"
#include "EnginePath.h" 
#include <filesystem>   
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "EngineState.h"
#include "Debugger.h"
#include "ImguiManager.h"
#include "SceneManager.h"
#include "NavMeshAgentProperties.h"
#include "NavMeshSystem.h"
#include "Imgui_Hierarchy.h"
#include "NavMeshProperties.h"
#include "FileCheckSystem.h"
#include "ScriptingSystem.h"
#include "ImGuizmo.h"
#include "fmod.h"
#include "fmod_studio.h"
#include <random> 
#include <filesystem>
#include "AudioSystem.h"
#include "AudioComponent.h"
#include "MessageSystem.h"
#include "ButtonSystem.h"


extern Framework::Coordinator ecsInterface;

ImVec4 helpYellow = ImVec4(1.0f, 0.9f, 0.4f, 1.0f);                 // soft golden-yellow
// "Global variables" For Victor and Joel for IMGUI portion
static int selectedModelIndex = -1;
static char meshNameBuffer[128] = "";                               // Buffer for the mesh name
static char imagePathBuffer[256] = "";                              // Buffer for the image path
static Entity selectedEntity = std::numeric_limits<Entity>::max();  // Sets a selected entity to be a non existent entity out of range
static bool isPropertiesWindowOpen = false;                         // Allows opening and closing of the name editor? might want to remove
static int  g_MainViewIndex = 0;                                    // Main view index for framebuffers
static bool g_prevF2 = false;                                       // Previous state of F2 key
static bool g_prevF3 = false;                                       // Previous state of F3 key

namespace fs = std::filesystem;

// Converts "Assets/Scripts/hello.lua" → "hello.lua"
static std::string GetFileNameOnly(const std::string& p)
{
    return fs::path(p).filename().string();
}

namespace Framework {
    float projWidth{};
    float projHeight{};
    GLuint gameFramebuffer{}, gameTexture{}, rbo{};
    bool print_out = false;
    static std::string filePath{};
    static std::vector<Framework::FileEvent> pendingEvents;
    //second framebuffer for game view
    GLuint gameFramebufferB{}, gameTextureB{}, rboB{};

    // Error message and flag for popup
    std::string errorMessage;
    bool showErrorPopup = false;

    // For Audio
    static TextureProperties g_AudioIcon;
    static bool g_AudioIconLoaded = false;

    static TextureProperties g_ObjIcon;
    static bool g_ObjIconLoaded = false;

    // --- Script file browser helpers ---
    static std::vector<std::string> g_scriptFiles;
    static std::vector<const char*> g_scriptFileItems;
    static std::vector<std::string> g_scriptDisplayNames; // short names: "hello.lua"

    //Initialize static instance 
    ImguiManager* ImguiManager::instance = nullptr;

    ImguiManager::ImguiManager() {
        instance = this;
        memset(filterBuffer, 0, sizeof(filterBuffer));
        m_graphicsSystem = nullptr;
        m_fboW = 1280;
        m_fboH = 720;
    }

    ImguiManager::~ImguiManager() {

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

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

    //FILE CHECKING
    void ImguiManager::DrawHotReloadPrompt()
    {
        auto fileCheckSystem = ecsInterface.GetSystem<Framework::FileCheckSystem>();
        auto scriptingSystem = ecsInterface.GetSystem<Framework::ScriptingSystem>();

        // Step 1: poll new file events
        Framework::FileEvent ev;
        static std::unordered_map<std::string, double> lastHandled;
        double now = ImGui::GetTime();
        int pulled = 0;
        while (fileCheckSystem->PopEvent(ev)) {
            ++pulled;

            if (m_autoReload)
            {
                // simple debounce (avoid multiple reloads per save)
                if (now - lastHandled[ev.path] > 0.35)
                {
                    lastHandled[ev.path] = now;

                    // Reload script files only
                    if (std::filesystem::path(ev.path).extension() == ".lua")
                    {
                        if (scriptingSystem)
                            scriptingSystem->HotReloadScript(ev.path);

                        std::cout << "[AutoReload] " << ev.path << "\n";
                    }
                }
            }
            else
            {
                // Prompt mode → push to pending
                pendingEvents.push_back(ev);
            }
        }

        if (m_autoReload) return;
        //std::cout << "[HotReload] pulled this frame: " << pulled << " | pendingEvents: " << pendingEvents.size() << "\n";

        // Step 2: draw prompt if any
        if (!pendingEvents.empty()) {
            ImGui::OpenPopup("Hot Reload");
        }
        if (ImGui::BeginPopupModal("Hot Reload", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            auto& ev = pendingEvents.front();
            ImGui::TextWrapped("File changed:\n%s", ev.path.c_str());
            if (ImGui::Button("Hot Reload")) {
                if (auto scripting = ecsInterface.GetSystem<Framework::ScriptingSystem>()) {
                    scripting->HotReloadScript(ev.path);
                }
                pendingEvents.erase(pendingEvents.begin());
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Ignore")) {
                pendingEvents.erase(pendingEvents.begin());
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
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
    //return FBO by view index (0 = grounded, 1 = free-fly)
    GLuint ImguiManager::GetGameFramebuffer(int viewIndex)
    {
        return (viewIndex == 0) ? gameFramebuffer : gameFramebufferB;
    }

    // Texture getter by view index so ImGui can display either view
    GLuint ImguiManager::GetGameTexture(int viewIndex)
    {
        return (viewIndex == 0) ? gameTexture : gameTextureB;
    }

    void ImguiManager::ResizeViewFBOs(int w, int h)
    {
        if (this == nullptr) return;
        if (!m_graphicsSystem) return;
        if (w <= 0 || h <= 0)   return;
        if (w == m_fboW && h == m_fboH) return;

        m_fboW = w;
        m_fboH = h;

        auto recreate = [&](GLuint& fb, GLuint& tex, GLuint& rbo) {
            // Delete the old objects AND zero out their handles
            if (fb) { glDeleteFramebuffers(1, &fb);  fb = 0; }
            if (tex) { glDeleteTextures(1, &tex);   tex = 0; }
            if (rbo) { glDeleteRenderbuffers(1, &rbo); rbo = 0; }

            // Now that tex and rbo are 0, CreateFramebuffer can
            // safely generate new ones and assign them back to the references.
            fb = m_graphicsSystem->CreateFramebuffer(m_fboW, m_fboH, tex, rbo);
            };

        recreate(gameFramebuffer, gameTexture, rbo);
        recreate(gameFramebufferB, gameTextureB, rboB);
    }

    void DropCallback(GLFWwindow* window, int count, const char** paths)
    {
        (void)window;
        std::filesystem::path root = GetEngineRootPath(); // <-- FIX: Get the root path once

        // <-- FIX: Define all destination folders as absolute paths
        const std::string textureFolder = (root / "Assets/Textures").string();
        const std::string audioFolder = (root / "Assets/Audio").string();
        const std::string meshFolder = (root / "Assets/Objects").string();
        const std::string sceneFolder = (root / "Scenes").string();
        const std::string prefabFolder = (root / "Assets/Prefabs").string();
        const std::string scriptFolder = (root / "Assets/Scripts").string();
        const std::string materialFolder = (root / "Assets/Materials").string();

        std::string finalAbsolutePath; // <-- FIX: Renamed for clarity
        std::string error;

        for (int i = 0; i < count; ++i)
        {
            std::string filePath = paths[i];
            std::string extension = filePath.substr(filePath.find_last_of('.') + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

            // Texture Files
            if (extension == "png" || extension == "jpg" || extension == "jpeg" ||
                extension == "bmp" || extension == "tga" || extension == "dds")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<TextureProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, textureFolder, finalAbsolutePath, error)) // <-- FIX: Using absolute folder
                    {
                        // <-- FIX: Create a relative path for the asset database
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string();

                        auto texPtr = std::make_unique<TextureProperties>();
                        texPtr->name = std::filesystem::path(fileName).stem().string();
                        texPtr->path = relativePath; // <-- FIX: Store the relative path

                        bool loaded = false;
                        if (extension == "dds")
                        {
                            GLuint texID = LoadDDS(finalAbsolutePath); // <-- FIX: Load from the new absolute path
                            if (texID)
                            {
                                texPtr->textureID = texID;
                                loaded = true;
                            }
                            else std::cerr << "[TextureLoader] Failed to load DDS: " << finalAbsolutePath << std::endl;
                        }
                        else
                        {
                            // Assumes loadTexture reads texPtr->path and builds its own absolute path
                            GLuint texID = TextureLoader::loadTexture(*texPtr, true);
                            if (texID)
                            {
                                loaded = true;
                            }
                            else std::cerr << "\n===============================\n[TextureLoader] Failed to load texture: " << finalAbsolutePath << "\n=======================\n" << std::endl;
                        }

                        if (loaded)
                        {
                            // <-- FIX: Register using the relative path as the key
                            AssetManager::Instance().RegisterAsset<TextureProperties>(std::move(texPtr), &g_TexturePropertiesProps, relativePath);

                            // <-- FIX: Save JSON to its absolute path
                            AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, (root / "Assets/JSON/TextureAsset.json").string(), "textures");
                            std::cout << "Texture added: " << fileName << " to " << relativePath << std::endl;
                        }
                        else
                        {
                            errorMessage = "Failed to load texture:\n" + finalAbsolutePath;
                            showErrorPopup = true;
                            return;
                        }
                    }
                }
                else std::cout << "Texture already exists: " << fileName << std::endl;
            }

            // Audio Files
            else if (extension == "mp3" || extension == "wav" || extension == "flac")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<AudioMetadata>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, audioFolder, finalAbsolutePath, error)) // <-- FIX: Using absolute folder
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string(); // <-- FIX

                        auto audioPtr = std::make_unique<AudioMetadata>();
                        audioPtr->name = std::filesystem::path(fileName).stem().string();
                        audioPtr->path = relativePath; // <-- FIX: Store the relative path
                        audioPtr->mode = "loop";
                        audioPtr->soundType = "background";

                        loadAudio(*audioPtr); // Assumes this uses the relative path in audioPtr

                        AssetManager::Instance().RegisterAsset<AudioMetadata>(std::move(audioPtr), &g_AudioMetadataProps, relativePath); // <-- FIX: Register with relative path

                        // <-- FIX: Save JSON to its absolute path
                        AssetManager::Instance().SaveArrayToJson(TypeName<AudioMetadata>::value, (root / "Assets/JSON/AudioAsset.json").string(), "audio"); // <-- FIX: Changed key "AudioProperties" to "audio"
                        std::cout << "Audio added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move audio:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Audio already exists: " << fileName << std::endl;
            }

            // OBJ || Mesh Files
            else if (extension == "obj")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<MeshProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, meshFolder, finalAbsolutePath, error)) // <-- FIX: Using absolute folder
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string(); // <-- FIX

                        auto meshPtr = std::make_unique<MeshProperties>();
                        meshPtr->name = std::filesystem::path(fileName).stem().string();
                        meshPtr->guid = GenerateGUID();
                        meshPtr->path = relativePath; // <-- FIX: Store the relative path

                        MeshLoader::loadMesh(finalAbsolutePath, *meshPtr); // <-- FIX: Load from absolute path

                        AssetManager::Instance().RegisterAsset<MeshProperties>(std::move(meshPtr), &g_MeshProps, relativePath); // <-- FIX: Register with relative path
                        // <-- FIX: Save JSON to its absolute path
                        AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, (root / "Assets/JSON/MeshAsset.json").string(), "mesh");
                        std::cout << "Mesh added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move mesh:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Mesh already exists: " << fileName << std::endl;
            }

            else if (extension == "fbx")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<AnimationProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, meshFolder, finalAbsolutePath, error)) // <-- FIX: Using absolute folder
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string(); // <-- FIX

                        auto meshPtr = std::make_unique<MeshProperties>();
                        auto AnimPtr = std::make_unique<AnimationProperties>();

                        meshPtr->name = std::filesystem::path(fileName).stem().string();
                        meshPtr->guid = GenerateGUID();
                        meshPtr->path = relativePath; // <-- FIX: Store the relative path

                        AnimPtr->name = std::filesystem::path(fileName).stem().string();
                        AnimPtr->guid = GenerateGUID();
                        AnimPtr->path = relativePath; // <-- FIX: Store the relative path

                        MeshLoader::loadAnimatedMesh(finalAbsolutePath, *AnimPtr, *meshPtr); // <-- FIX: Load from absolute
                        AnimationLoader::loadAnimation(finalAbsolutePath, *AnimPtr); // <-- FIX: Load from absolute

                        AssetManager::Instance().RegisterAsset<MeshProperties>(std::move(meshPtr), &g_MeshProps, relativePath); // <-- FIX: Register with relative path
                        // Save the MESH JSON
                        AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, (root / "Assets/JSON/MeshAsset.json").string(), "mesh"); // <-- FIX: Absolute path
                        AssetManager::Instance().RegisterAsset<AnimationProperties>(std::move(AnimPtr), &g_AnimationPropertiesProps,relativePath ); // <-- FIX: Register with relative path

                        // Save the ANIMATION JSON
                        // <-- FIX: Changed key "animations" to "animation" 
                        // <-- FIX: Absolute path
                        AssetManager::Instance().SaveArrayToJson(TypeName<AnimationProperties>::value,(root / "Assets/JSON/AnimationAsset.json").string(), "animation" );
                        std::cout << "Mesh added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move mesh:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Mesh already exists: " << fileName << std::endl;
            }

            // Scene Files
            else if (extension == "scene")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<SceneProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, sceneFolder, finalAbsolutePath, error)) // <-- FIX: Using absolute folder
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string(); // <-- FIX

                        auto scenePtr = std::make_unique<SceneProperties>();
                        scenePtr->name = std::filesystem::path(fileName).stem().string();
                        scenePtr->path = relativePath; // <-- FIX: Store the relative path
                        scenePtr->guid = GenerateGUID();
                        AssetManager::Instance().RegisterAsset<SceneProperties>(std::move(scenePtr), &g_SceneProps, relativePath); // <-- FIX: Register with relative path
                        // <-- FIX: Save JSON to its absolute path
                        AssetManager::Instance().SaveArrayToJson(TypeName<SceneProperties>::value, (root / "Assets/JSON/SceneAsset.json").string(), "scenes");
                        std::cout << "Scene added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move scene:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Scene already exists: " << fileName << std::endl;
            }

            // Prefab Files
            else if (extension == "prefab")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<PrefabProperties>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, prefabFolder, finalAbsolutePath, error)) // <-- FIX: Using absolute folder
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string(); // <-- FIX

                        auto prefabPtr = std::make_unique<PrefabProperties>();
                        prefabPtr->name = std::filesystem::path(fileName).stem().string();
                        prefabPtr->path = relativePath; // <-- FIX: Store the relative path
                        prefabPtr->guid = GenerateGUID();
                        prefabPtr->isScenePrefab = false;

                        AssetManager::Instance().RegisterAsset<PrefabProperties>(std::move(prefabPtr), &g_PrefabProps, relativePath); // <-- FIX: Register with relative path
                        // <-- FIX: Save JSON to its absolute path
                        AssetManager::Instance().SaveArrayToJson(TypeName<PrefabProperties>::value, (root / "Assets/JSON/PrefabAsset.json").string(), "prefabs");
                        std::cout << "Prefab added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move prefab:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Prefab already exists: " << fileName << std::endl;
            }

            // Script Files
            else if (extension == "lua")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<ScriptEntry>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, scriptFolder, finalAbsolutePath, error))
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string();

                        auto scriptPtr = std::make_unique<ScriptEntry>();
                        scriptPtr->scriptPath = relativePath;
                        scriptPtr->guid = GenerateGUID();

                        AssetManager::Instance().RegisterAsset<ScriptEntry>(std::move(scriptPtr), &g_ScriptEntryProps, relativePath);
                        AssetManager::Instance().SaveArrayToJson(TypeName<ScriptEntry>::value, (root / "Assets/JSON/ScriptAsset.json").string(), "scripts");
                        std::cout << "Script added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move script:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Script already exists: " << fileName << std::endl;
            }

             // Material Files
            else if (extension == "material")
            {
                auto existingGuid = AssetManager::Instance().FindGuid<MaterialMetadata>(fileName);
                if (existingGuid.empty())
                {
                    if (MoveFileToAssetFolder(filePath, materialFolder, finalAbsolutePath, error)) // <-- FIX
                    {
                        std::string relativePath = std::filesystem::relative(finalAbsolutePath, root).string(); // <-- FIX

                        auto materialPtr = std::make_unique<MaterialProperties>();
                        materialPtr->guid = GenerateGUID();

                        // Load material data from file
                        auto loaded = LoadMaterialAsset(finalAbsolutePath, materialPtr->guid); // <-- FIX
                        if (loaded)
                        {
                            materialPtr->ambient = loaded->ambient;
                            materialPtr->albedo = loaded->albedo;
                            materialPtr->specular = loaded->specular;
                            materialPtr->shininess = loaded->shininess;
                            materialPtr->opacity = loaded->opacity;
                        }

                        // Register metadata
                        auto meta = std::make_unique<MaterialMetadata>();
                        meta->guid = materialPtr->guid;
                        meta->name = std::filesystem::path(fileName).stem().string();
                        meta->path = relativePath; // <-- FIX

                        const prop_list* props = TypeRegistry::Instance().GetPropList(TypeName<MaterialMetadata>::value);
                        AssetManager::Instance().RegisterAsset(std::move(meta), props, relativePath); // <-- FIX
                        // <-- FIX
                        AssetManager::Instance().SaveArrayToJson(TypeName<MaterialMetadata>::value, (root / "Assets/JSON/MaterialAsset.json").string(), "materials"); // <-- FIX: Changed key "material" to "materials"
                        std::cout << "Material added: " << fileName << " to " << relativePath << std::endl;
                    }
                    else
                    {
                        errorMessage = "Failed to move material:\n" + error;
                        showErrorPopup = true;
                        return;
                    }
                }
                else std::cout << "Material already exists: " << fileName << std::endl;
            }
        }
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

        /*gameFramebuffer = m_graphicsSystem->CreateFramebuffer(static_cast<int>(1280), static_cast<int>(720), gameTexture, rbo);
        gameFramebufferB = m_graphicsSystem->CreateFramebuffer(static_cast<int>(1280), static_cast<int>(720), gameTextureB, rboB);*/
        int w = 0, h = 0;
        glfwGetFramebufferSize(m_graphicsSystem->GetWindow(), &w, &h);
        m_fboW = std::max(1, w);
        m_fboH = std::max(1, h);

        gameFramebuffer = m_graphicsSystem->CreateFramebuffer(m_fboW, m_fboH, gameTexture, rbo);
        gameFramebufferB = m_graphicsSystem->CreateFramebuffer(m_fboW, m_fboH, gameTextureB, rboB);

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

        //Subsribe to console messages 
        MessageBus::Get().Subscribe<ConsoleLogMessage>(
            [this](const ConsoleLogMessage& msg) {OnConsoleLog(msg);});

        Log("ImguiManager initialized", "ImGui");
    }

    void ImguiManager::Update(float deltaTime) {


        // Toggle editor visibility with Backspace key
        static bool backspaceWasPressed = false;
        bool backspaceIsPressed = InputHandler::GetInstance()->IsKeyHeld(GLFW_KEY_1);

        if (backspaceIsPressed && !backspaceWasPressed)
        {
            m_showEditor = !m_showEditor;
            if (m_showEditor) {
                std::cout << "[ImGui] Editor enabled" << std::endl;

                // Stop BGM when returning to editor
                if (g_AudioSystem) {
                    g_AudioSystem->Reset();
                    std::cout << "[Audio] BGM Stopped (returned to editor)" << std::endl;
                }
            }
            else {
                std::cout << "[ImGui] Editor disabled - Game only mode" << std::endl;

                // Start BGM when entering fullscreen game mode
                if (g_AudioSystem) {
                    std::string bgmPath = sceneManager.GetCurrentSceneBGMPath();
                    if (!bgmPath.empty()) {
                        g_AudioSystem->PlayAudio(bgmPath);
                        std::cout << "[Audio] Playing BGM: " << bgmPath << std::endl;
                    }
                }
            }
        }
        backspaceWasPressed = backspaceIsPressed;

        // Always start a new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int w, h;
        glfwGetFramebufferSize(m_graphicsSystem->GetWindow(), &w, &h);

        // JAY
        // Handle F2 / F3 to swap which view is "Main"
        GLFWwindow* win = m_graphicsSystem->GetWindow();
        bool nowF2 = (glfwGetKey(win, GLFW_KEY_F2) == GLFW_PRESS);
        bool nowF3 = (glfwGetKey(win, GLFW_KEY_F3) == GLFW_PRESS);

        // On rising edge of F2: main shows view 0
        if (nowF2 && !g_prevF2) {
            g_MainViewIndex = 1;
            m_graphicsSystem->SetMouseTargetView(1);
            m_graphicsSystem->SetPickViewIndex(g_MainViewIndex);
        }

        // On rising edge of F3: main shows view 0
        if (nowF3 && !g_prevF3) {
            g_MainViewIndex = 0;
            m_graphicsSystem->SetMouseTargetView(0);
            m_graphicsSystem->SetPickViewIndex(g_MainViewIndex);
        }

        g_prevF2 = nowF2;
        g_prevF3 = nowF3;

        // Render based on editor visibility
        if (m_showEditor)
        {
            // === EDITOR MODE: Render full ImGui UI ===
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

                windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
                windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

                ImGui::Begin("DockSpace Demo", nullptr, windowFlags);
                ImGui::PopStyleVar(2);

                ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
                ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

                RenderMenuBar();
                RenderEntityMenu();
                DrawHotReloadPrompt();
                RenderGizmoPanel();
                RenderAssetPanel();
                ImGui::End();
            }

            if (showConsole) RenderConsole();
        }
        else
        {
            // === GAME ONLY MODE: Render game fullscreen ===
            if (w > 0 && h > 0)
            {
                // Create a fullscreen window with no decorations
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->WorkPos);
                ImGui::SetNextWindowSize(viewport->WorkSize);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

                ImGui::Begin("GameFullscreen", nullptr, windowFlags);

                // Get the available size
                ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                float aspectRatio = (m_fboH > 0) ? (float)m_fboW / (float)m_fboH : 16.0f / 9.0f;
                float newWidth = viewportSize.x;
                float newHeight = newWidth / aspectRatio;

                if (newHeight > viewportSize.y)
                {
                    newHeight = viewportSize.y;
                    newWidth = newHeight * aspectRatio;
                }

                // Center the image
                ImVec2 offset((viewportSize.x - newWidth) * 0.5f, (viewportSize.y - newHeight) * 0.5f);
                ImGui::SetCursorPos(offset);

                // Render the game texture fullscreen
                ImGui::Image(
                    (ImTextureID)(intptr_t)GetGameTexture(g_MainViewIndex),
                    ImVec2(newWidth, newHeight),
                    ImVec2(0, 1),
                    ImVec2(1, 0)
                );

                ImGui::End();
                ImGui::PopStyleVar(2);
            }
        }

        // Always render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Handle multiple viewports
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);

        glfwSetDropCallback(backup_current_context, DropCallback);
        glfwPollEvents();
        glfwSwapBuffers(m_graphicsSystem->GetWindow());
    }

    //Console Message handler 
    void ImguiManager::OnConsoleLog(const ConsoleLogMessage& msg) {
        ConsoleEntry entry;
        entry.category = msg.category;
        entry.message = msg.message;
        entry.level = msg.level;

        // GENERATE TIMESTAMP HERE (not copy from msg)
        time_t now = time(nullptr);
        char buffer[32];
#ifdef _WIN32
        struct tm timeInfo;
        localtime_s(&timeInfo, &now);
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeInfo);
#else
        struct tm* timeInfo = localtime(&now);
        strftime(buffer, sizeof(buffer), "%H:%M:%S", timeInfo);
#endif
        entry.timestamp = std::string(buffer);

        consoleBuffer.push_back(entry);

        //Maintain buffer size 
        while (consoleBuffer.size() > maxBufferSize) {
            consoleBuffer.pop_front();
        }

#ifdef _DEBUG
        //Also print to stdout in debug builds 
        std::cout << "[" << entry.timestamp << "] "
            << entry.category << ": "  // FIX: Added space after colon
            << entry.message << std::endl;
#endif 

    }

    //Render Console Window 
    void ImguiManager::RenderConsole() {
        ImGui::Begin("Console Window", &showConsole, ImGuiWindowFlags_MenuBar);

        //Menu Bar 
        if (ImGui::BeginMenuBar()) {
            if (ImGui::Button("Clear")) {
                consoleBuffer.clear();
            }

            ImGui::Separator();
            ImGui::Checkbox("Auto-scroll", &autoScroll);
            ImGui::Separator();
            ImGui::Checkbox("Info", &showInfo);
            ImGui::SameLine();
            ImGui::Checkbox("Warning", &showWarning);
            ImGui::SameLine();
            ImGui::Checkbox("Error", &showError);
            ImGui::SameLine();
            ImGui::Checkbox("Success", &showSuccess);
            ImGui::EndMenuBar();
        }

        //Filter input 
        ImGui::InputText("Filter", filterBuffer, sizeof(filterBuffer));
        ImGui::Separator();

        // Console output area
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false,
            ImGuiWindowFlags_HorizontalScrollbar);

        std::string filterStr(filterBuffer);

        for (const auto& entry : consoleBuffer) {
            // Apply level filters
            if (entry.level == ConsoleLogMessage::LogLevel::INFO && !showInfo) continue;
            if (entry.level == ConsoleLogMessage::LogLevel::WARNING && !showWarning) continue;
            if (entry.level == ConsoleLogMessage::LogLevel::ERR && !showError) continue;
            if (entry.level == ConsoleLogMessage::LogLevel::SUCCESS && !showSuccess) continue;

            // Apply text filter
            if (!filterStr.empty()) {
                if (entry.message.find(filterStr) == std::string::npos &&
                    entry.category.find(filterStr) == std::string::npos) {
                    continue;
                }
            }

            // Colorize based on level
            glm::vec4 color = GetColorForLevel(entry.level);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, color.a));

            // Format: [timestamp] CATEGORY: message
            std::string displayText = "[" + entry.timestamp + "] " +
                entry.category + ": " +
                entry.message;
            ImGui::TextUnformatted(displayText.c_str());

            ImGui::PopStyleColor();
        }

        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

    //Get color for log level 
    glm::vec4 ImguiManager::GetColorForLevel(ConsoleLogMessage::LogLevel level) {
        switch (level) {
        case ConsoleLogMessage::LogLevel::INFO:
            return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); //white 
        case ConsoleLogMessage::LogLevel::WARNING:
            return glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        case ConsoleLogMessage::LogLevel::ERR:
            return glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
        case ConsoleLogMessage::LogLevel::SUCCESS:
            return glm::vec4(0.2f, 1.0f, 0.2f, 1.0f); // Green
        default:
            return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    //Static logging methods 
    void ImguiManager::Log(const std::string& message, const std::string& category) {
        MessageBus::Get().SendImmediate(
            ConsoleLogMessage(message, ConsoleLogMessage::LogLevel::INFO, category));
    }

    void ImguiManager::LogWarning(const std::string& message, const std::string& category) {
        MessageBus::Get().SendImmediate(
            ConsoleLogMessage(message, ConsoleLogMessage::LogLevel::WARNING, category));
    }

    void ImguiManager::LogError(const std::string& message, const std::string& category) {
        MessageBus::Get().SendImmediate(
            ConsoleLogMessage(message, ConsoleLogMessage::LogLevel::ERR, category));
    }

    void ImguiManager::LogSuccess(const std::string& message, const std::string& category) {
        MessageBus::Get().SendImmediate(
            ConsoleLogMessage(message, ConsoleLogMessage::LogLevel::SUCCESS, category));
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

                    std::filesystem::current_path(GetEngineRootPath());
                }
                if (ImGui::MenuItem("Save"))
                {
                    std::string savePath = SaveFileDialog();
                    std::cout << "\n============================\n";
                    std::cout << savePath;
                    std::cout << "\n============================\n";
                    Framework::sceneManager.SaveScene(savePath);
                }

                ImGui::Separator();  // Visual separator

                // === PREFAB TEST MENU ===
                if (ImGui::MenuItem("Test Spawn Prefab"))
                {
                    // Find first available prefab and spawn it
                    bool spawned = false;
                    for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
                    {
                        if (entry.typeName != TypeName<PrefabProperties>::value)
                            continue;

                        auto* prefab = static_cast<PrefabProperties*>(entry.pObject.get());

                        // Spawn at origin
                        Entity entity = Framework::sceneManager.SpawnPrefab(
                            prefab->guid.id
                        );

                        if (entity != std::numeric_limits<Entity>::max())
                        {
                            std::cout << "✓ Test spawned prefab: " << prefab->name
                                << " (entity " << entity << ")" << std::endl;
                            spawned = true;
                        }
                        break;  // Only spawn first prefab
                    }

                    if (!spawned)
                    {
                        std::cout << "⚠ No prefabs available to spawn. Create one first!" << std::endl;
                    }
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

                            // Random number generator setup
                            std::random_device rd;
                            std::mt19937 gen(rd());
                            std::uniform_real_distribution<float> dist(-5.0f, 5.0f);

                            glm::vec3 destination;
                            destination.x = dist(gen); // random range between 5 and -5 for testing
                            destination.y = 1.0f;
                            destination.z = 2.5f;

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

                ImGui::EndMenu();
                //Add Windows menu to toggle console
                if (ImGui::BeginMenu("Windows")) {
                    ImGui::MenuItem("Console Window", nullptr, &showConsole);
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenuBar();
        }
        //jay
        //ImGui::End();
    }

    void ImguiManager::RenderFolderTree(const AssetFolder& folder)
    {
        bool open = folder.children.empty()
            ? ImGui::TreeNodeEx(folder.name.c_str(), ImGuiTreeNodeFlags_Leaf)
            : ImGui::TreeNode(folder.name.c_str());

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && folder.type != AssetTabType::None)
        {
            openTabs.push_back({ folder.type, folder.name });
        }

        if (open)
        {
            if (!folder.children.empty())
            {
                for (const auto& child : folder.children)
                    RenderFolderTree(child);
            }
            ImGui::TreePop(); // Always pop if open
        }
    }

    void ImguiManager::RenderAssetPanel()
    {
        ImGui::Begin("Project");

        // Split the panel into two child regions: left (explorer) and right (tabs)
        ImGui::Columns(2, "Asset Explorer", true);

        // Left: Folder Explorer
        ImGui::BeginChild("FolderExplorer", ImVec2(0, 0), true);
        for (const auto& root : assetTree) RenderFolderTree(root);
        ImGui::EndChild();

        ImGui::NextColumn();

        // Right: Asset Tabs
        ImGui::BeginChild("AssetTabs", ImVec2(0, 0), true);
        if (ImGui::BeginTabBar("AssetTabBar")) 
        {
            for (int i = 0; i < openTabs.size(); ++i) 
            {
                const auto& tab = openTabs[i];
                if (ImGui::BeginTabItem(tab.label.c_str())) 
                {
                    switch (tab.type) 
                    {
                    case AssetTabType::Mesh:
                        RenderMeshAssets();
                        break;
                    case AssetTabType::Texture:
                        RenderTextureAssets();
                        break;
                    case AssetTabType::Animation:
                        RenderAnimationAssets();
                        break;
                    case AssetTabType::Audio:
                        RenderAudioAssets();
                        break;
                    case AssetTabType::Prefab:
                        RenderPrefabAssets();
                        break;
                    case AssetTabType::Scene:
                        RenderSceneAssets();
                        break;
                    case AssetTabType::Scripts:
                        RenderScriptAssets();
                        break;
                    default:
                        std::cerr << "Unknown AssetTabType: " << static_cast<int>(tab.type) << std::endl;
                        break;
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void ImguiManager::RenderMeshAssets() 
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

            ImGui::PushID(mesh->guid.id.c_str());
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
                        mesh->name = newName;
                    }
                    initialized = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine(0, spacing);
                if (ImGui::Button("Delete", ImVec2(buttonWidth, 0)))
                {
                    meshesToDelete.push_back(mesh->guid.id.c_str());
                    initialized = false;
                    
                    // Delete the actual mesh file
                    if (!AssetManager::Instance().DeleteAssetFile(mesh->path))
                    {
                        errorMessage = "Failed to delete mesh file:\n" + mesh->path;
                        showErrorPopup = true;
                    }
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
                ImGui::Text("GUID: %s", mesh->guid.id.c_str());
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

        if (!meshesToDelete.empty())
        {
            for (const auto& guid : meshesToDelete)
            {
                AssetManager::Instance().Unload(guid);
            }
            AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, "Assets/JSON/MeshAsset.json", "mesh");
        }
        ImGui::NewLine();
        RenderErrorPopup();
    }

    void ImguiManager::RenderTextureAssets() 
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

            ImGui::PushID(tex->guid.id.c_str());
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
                if (renameBuffers.find(tex->guid.id.c_str()) == renameBuffers.end())
                {
                    std::snprintf(renameBuffers[tex->guid.id.c_str()], sizeof(renameBuffers[tex->guid.id.c_str()]), "%s", tex->name.c_str());
                }

                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Texture Options");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Rename Texture:");
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
                ImGui::InputText("##Rename", renameBuffers[tex->guid.id.c_str()], IM_ARRAYSIZE(renameBuffers[tex->guid.id.c_str()]));

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
                    std::string newName = renameBuffers[tex->guid.id.c_str()];
                    if (newName != tex->name)
                    {
                        tex->name = newName;
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine(0, spacing);
                if (ImGui::Button("Delete", ImVec2(buttonWidth, 0)))
                {
                    texturesToDelete.push_back(tex->guid.id.c_str());

                    // Delete the actual file
                    if (!AssetManager::Instance().DeleteAssetFile(tex->path))
                    {
                        errorMessage = "Failed to delete texture file:\n" + tex->path;
                        showErrorPopup = true;
                    }
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
                ImGui::Text("GUID: %s", tex->guid.id.c_str());
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

        if (!texturesToDelete.empty())
        {
            for (const auto& guid : texturesToDelete)
            {
                AssetManager::Instance().Unload(guid);
            }
            AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, "Assets/JSON/TextureAsset.json", "textures");
        }
        ImGui::NewLine();
        RenderErrorPopup();
    }

    // Enhanced RenderAudioAssets method for ImguiManager.cpp
// Replace the existing RenderAudioAssets method (lines 1577-1797) with this code:

    void ImguiManager::RenderAudioAssets()
    {
        // Enhanced Unity-like Audio Panel Header
        ImGui::TextColored(helpYellow, "(❓) Unity-Style Audio Manager");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(
                "Unity-Style Audio Management:\n\n"
                "- Audio Browser displays all imported audio clips with metadata\n"
                "- Search and filter by type (Music, SFX, Ambient, UI)\n"
                "- Preview Panel with play/stop and volume control\n"
                "- Right-click to:\n"
                "    • Add to selected entity (creates AudioComponent)\n"
                "    • Edit properties (name, mode, type)\n"
                "    • Delete audio asset\n\n"
                "3D Audio Testing:\n"
                "- Add AudioComponent to entities for 3D positioning\n"
                "- Configure min/max distance for falloff\n"
                "- Add ListenerComponent to player for spatial audio\n\n"
                "Status Indicators:\n"
                "- 🟢 Green = Currently playing\n"
                "- 🔵 Blue = 3D audio source\n"
                "- 🟡 Yellow = Looping audio\n");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // Search and Filter Bar
        static char searchFilter[256] = "";
        static int audioTypeFilter = 0; // 0=All, 1=Music, 2=SFX, 3=Ambient, 4=UI
        static AssetHandle selectedAudioGuid;
        static FMOD::Studio::EventInstance* previewInstance = nullptr;
        static float previewVolume = 1.0f;
        static bool isPlaying = false;

        ImGui::Separator();

        // Search Bar
        ImGui::SetNextItemWidth(300);
        ImGui::InputTextWithHint("##AudioSearch", "🔍 Search audio files...", searchFilter, sizeof(searchFilter));
        ImGui::SameLine();

        // Type Filter
        const char* audioTypes[] = { "All", "Music", "SFX", "Ambient", "UI" };
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("Type Filter", &audioTypeFilter, audioTypes, IM_ARRAYSIZE(audioTypes));
        ImGui::SameLine();

        // Add to Entity Button (Unity-style)
        if (ImGui::Button("Add to Selected Entity"))
        {
            if (selectedAudioGuid && selectedEntity != std::numeric_limits<Entity>::max())
            {
                if (!ecsInterface.HasComponent<AudioComponent>(selectedEntity))
                {
                    AudioMetadata* meta = AssetManager::Instance().Get<AudioMetadata>(selectedAudioGuid);
                    if (meta)
                    {
                        AudioComponent audioComp;
                        audioComp.guid = selectedAudioGuid;
                        audioComp.eventName = meta->name;
                        audioComp.is3D = true;
                        audioComp.minDistance = 1.0f;
                        audioComp.maxDistance = 50.0f;
                        audioComp.volume = 1.0f;
                        audioComp.playOnStart = false;
                        ecsInterface.AddComponent(selectedEntity, audioComp);

                        LogSuccess("Added AudioComponent to entity " + std::to_string(selectedEntity), "Audio");
                    }
                }
                else
                {
                    LogWarning("Entity already has an AudioComponent", "Audio");
                }
            }
            else
            {
                LogWarning("Select both an audio file and an entity first", "Audio");
            }
        }

        ImGui::Separator();

        // Preview Controls Bar
        if (ImGui::BeginChild("AudioPreviewControls", ImVec2(0, 70), true))
        {
            // Play/Stop button
            if (ImGui::Button(isPlaying ? "⏹ Stop" : "▶ Play", ImVec2(80, 40)))
            {
                if (isPlaying && previewInstance)
                {
                    previewInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                    previewInstance->release();
                    previewInstance = nullptr;
                    isPlaying = false;
                    std::cout << "[UI] Stopped preview playback" << std::endl;
                }
                else if (selectedAudioGuid)
                {
                    AudioMetadata* audioMeta = AssetManager::Instance().Get<AudioMetadata>(selectedAudioGuid);
                    if (audioMeta)
                    {
                        std::cout << "[UI] Preview Play - Name: " << audioMeta->name << std::endl;
                        std::cout << "[UI] Preview Play - Path: " << audioMeta->path << std::endl;

                        // Load event
                        FMOD::Studio::EventDescription* eventDesc = g_AudioSystem->LoadEvent(audioMeta->path);
                        if (eventDesc)
                        {
                            // Create instance
                            FMOD_RESULT result = eventDesc->createInstance(&previewInstance);
                            if (result == FMOD_OK && previewInstance)
                            {
                                // ✅ Check if 3D and set position
                                bool is3D = false;
                                eventDesc->is3D(&is3D);
                                std::cout << "[UI] Event is 3D: " << (is3D ? "Yes" : "No") << std::endl;

                                if (is3D)
                                {
                                    // Set position at listener (origin)
                                    FMOD_3D_ATTRIBUTES attributes = {};
                                    attributes.position = { 0.0f, 0.0f, 0.0f };
                                    attributes.velocity = { 0.0f, 0.0f, 0.0f };
                                    attributes.forward = { 0.0f, 0.0f, 1.0f };
                                    attributes.up = { 0.0f, 1.0f, 0.0f };
                                    previewInstance->set3DAttributes(&attributes);
                                }

                                // Set volume
                                previewInstance->setVolume(previewVolume);

                                // Start playback
                                result = previewInstance->start();
                                if (result == FMOD_OK)
                                {
                                    isPlaying = true;
                                    std::cout << "[UI] Preview started successfully" << std::endl;

                                    // ✅ CRITICAL: Give FMOD time to process
                                    for (int i = 0; i < 5; i++)
                                    {
                                        if (g_AudioSystem && g_AudioSystem->GetStudioSystem()) {
                                            g_AudioSystem->GetStudioSystem()->update();
                                        }
                                    }

                                    // Check state
                                    FMOD_STUDIO_PLAYBACK_STATE state;
                                    previewInstance->getPlaybackState(&state);
                                    std::cout << "[UI] Playback state: " << state << std::endl;
                                }
                                else
                                {
                                    std::cerr << "[UI] Failed to start. Error: " << result << std::endl;
                                    previewInstance->release();
                                    previewInstance = nullptr;
                                }
                            }
                            else
                            {
                                std::cerr << "[UI] Failed to create instance. Error: " << result << std::endl;
                            }
                        }
                        else
                        {
                            std::cerr << "[UI] Failed to load event description" << std::endl;
                        }
                    }
                }
            }

            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("Preview Volume:");
            ImGui::SetNextItemWidth(200);
            if (ImGui::SliderFloat("##PreviewVolume", &previewVolume, 0.0f, 1.0f, "%.2f"))
            {
                if (previewInstance)
                {
                    previewInstance->setVolume(previewVolume);
                }
            }
            ImGui::EndGroup();

            ImGui::SameLine(0, 20);
            ImGui::BeginGroup();
            ImGui::Text("Selected: %s",
                selectedAudioGuid ?
                AssetManager::Instance().Get<AudioMetadata>(selectedAudioGuid)->name.c_str() :
                "None");
            if (selectedAudioGuid)
            {
                AudioMetadata* meta = AssetManager::Instance().Get<AudioMetadata>(selectedAudioGuid);
                if (meta)
                {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                        "Duration: %.2fs | Type: %s | Mode: %s",
                        meta->duration, meta->soundType.c_str(), meta->mode.c_str());
                }
            }
            ImGui::EndGroup();
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Audio Asset Table (Unity-style)
        const auto& assetsImgui = AssetManager::Instance().GetAssets();
        std::vector<std::string> audioToDelete;

        if (ImGui::BeginChild("AudioAssetList", ImVec2(0, 0), true))
        {
            if (ImGui::BeginTable("AudioTable", 5,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
            {
                // Column setup
                ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 40);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 200);
                ImGui::TableHeadersRow();

                for (const auto& [key, entry] : assetsImgui)
                {
                    if (entry.typeName != TypeName<AudioMetadata>::value)
                        continue;

                    AudioMetadata* audioMeta = static_cast<AudioMetadata*>(entry.pObject.get());
                    if (!audioMeta) continue;

                    // Apply search filter
                    if (searchFilter[0] != '\0')
                    {
                        std::string nameLower = audioMeta->name;
                        std::string filterLower = searchFilter;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                        if (nameLower.find(filterLower) == std::string::npos)
                            continue;
                    }

                    // Apply type filter
                    if (audioTypeFilter > 0)
                    {
                        const char* typeNames[] = { "", "Music", "SFX", "Ambient", "UI" };
                        if (audioMeta->soundType != typeNames[audioTypeFilter])
                            continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::PushID(audioMeta->guid.id.c_str());

                    // Column 0: Icon
                    ImGui::TableSetColumnIndex(0);
                    bool isSelected = (selectedAudioGuid == key.guid);

                    // Selectable row - THIS WAS MISSING!
                    if (ImGui::Selectable("##sel", isSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(0, 32)))
                    {
                        selectedAudioGuid = key.guid;

                        // Double click to play
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            std::cout << "[UI] Double-clicked: " << audioMeta->name << std::endl;

                            if (previewInstance)
                            {
                                previewInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                                previewInstance->release();
                                previewInstance = nullptr;
                            }

                            // Use the path field for the full event path
                            FMOD::Studio::EventDescription* eventDesc = g_AudioSystem->LoadEvent(audioMeta->path);
                            if (eventDesc)
                            {
                                FMOD_RESULT result = eventDesc->createInstance(&previewInstance);
                                if (result == FMOD_OK && previewInstance)
                                {
                                    previewInstance->setVolume(previewVolume);
                                    result = previewInstance->start();
                                    if (result == FMOD_OK)
                                    {
                                        isPlaying = true;
                                        std::cout << "[UI] Double-click play successful" << std::endl;
                                    }
                                    else
                                    {
                                        std::cerr << "[UI] Double-click failed to start. Error: " << result << std::endl;
                                        previewInstance->release();
                                        previewInstance = nullptr;
                                    }
                                }
                                else
                                {
                                    std::cerr << "[UI] Double-click failed to create instance. Error: " << result << std::endl;
                                }
                            }
                            else
                            {
                                std::cerr << "[UI] Double-click failed to load event" << std::endl;
                            }
                        }
                    }

                    // ✅ ATTACH CONTEXT MENU TO THE SELECTABLE
                    if (ImGui::BeginPopupContextItem())
                    {
                        ImGui::Text("Audio: %s", audioMeta->name.c_str());
                        ImGui::Separator();

                        if (ImGui::MenuItem("▶ Play"))
                        {
                            std::cout << "[UI] Play button clicked for: " << audioMeta->name << std::endl;
                            std::cout << "[UI] Event path: " << audioMeta->path << std::endl;

                            // Use the path field which has the full FMOD event path (e.g., "event:/ForestAmbient")
                            g_AudioSystem->PlayAudio(audioMeta->path);
                        }

                        if (ImGui::MenuItem("➕ Add to Selected Entity"))
                        {
                            if (selectedEntity != std::numeric_limits<Entity>::max())
                            {
                                if (!ecsInterface.HasComponent<AudioComponent>(selectedEntity))
                                {
                                    AudioComponent audioComp;
                                    audioComp.guid = key.guid;
                                    audioComp.eventName = audioMeta->name;
                                    audioComp.is3D = true;
                                    audioComp.playOnStart = false;
                                    ecsInterface.AddComponent(selectedEntity, audioComp);
                                    LogSuccess("Added audio to entity " + std::to_string(selectedEntity), "Audio");
                                }
                            }
                        }

                        ImGui::Separator();

                        // Edit name
                        static char renameBuffer[256];
                        strcpy_s(renameBuffer, audioMeta->name.c_str());
                        if (ImGui::InputText("Rename", renameBuffer, sizeof(renameBuffer)))
                        {
                            audioMeta->name = renameBuffer;
                            AssetManager::Instance().MarkDirty(audioMeta->guid);
                        }

                        // Edit type
                        int typeIndex = 0;
                        if (audioMeta->soundType == "Music") typeIndex = 1;
                        else if (audioMeta->soundType == "SFX") typeIndex = 2;
                        else if (audioMeta->soundType == "Ambient") typeIndex = 3;
                        else if (audioMeta->soundType == "UI") typeIndex = 4;

                        if (ImGui::Combo("Type", &typeIndex, audioTypes, IM_ARRAYSIZE(audioTypes)))
                        {
                            audioMeta->soundType = audioTypes[typeIndex];
                            AssetManager::Instance().MarkDirty(audioMeta->guid);
                        }

                        // Edit mode
                        int modeIndex = (audioMeta->mode == "loop") ? 1 : 0;
                        const char* modes[] = { "oneshot", "loop" };
                        if (ImGui::Combo("Mode", &modeIndex, modes, 2))
                        {
                            audioMeta->mode = modes[modeIndex];
                            AssetManager::Instance().MarkDirty(audioMeta->guid);
                        }

                        ImGui::Separator();

                        if (ImGui::MenuItem("🗑 Delete", nullptr, false, true))
                        {
                            audioToDelete.push_back(audioMeta->guid.id);
                        }

                        ImGui::EndPopup();
                    }

                    // Draw icon on top of selectable
                    ImGui::SameLine();
                    if (g_AudioIconLoaded && g_AudioIcon.textureID != 0)
                    {
                        ImGui::Image((ImTextureID)(intptr_t)g_AudioIcon.textureID, ImVec2(32, 32));
                    }
                    else
                    {
                        ImGui::Text("🔊");
                    }

                    // Tooltip on hover
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("GUID: %s", audioMeta->guid.id.c_str());
                        ImGui::Text("Path: %s", audioMeta->path.c_str());
                        ImGui::Text("Mode: %s", audioMeta->mode.c_str());
                        ImGui::Text("Type: %s", audioMeta->soundType.c_str());
                        ImGui::Text("Duration: %.2f seconds", audioMeta->duration);
                        ImGui::EndTooltip();
                    }

                    // Column 1: Name
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", audioMeta->name.c_str());
                    if (isPlaying && selectedAudioGuid == key.guid)
                    {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "♫");
                    }

                    // Column 2: Type
                    ImGui::TableSetColumnIndex(2);
                    ImVec4 typeColor = ImVec4(1, 1, 1, 1);
                    if (audioMeta->soundType == "Music") typeColor = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
                    else if (audioMeta->soundType == "SFX") typeColor = ImVec4(1.0f, 0.8f, 0.5f, 1.0f);
                    else if (audioMeta->soundType == "Ambient") typeColor = ImVec4(0.8f, 1.0f, 0.5f, 1.0f);
                    else if (audioMeta->soundType == "UI") typeColor = ImVec4(1.0f, 0.5f, 0.8f, 1.0f);
                    ImGui::TextColored(typeColor, "%s", audioMeta->soundType.c_str());

                    // Column 3: Duration
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.2fs", audioMeta->duration);

                    // Column 4: Actions
                    ImGui::TableSetColumnIndex(4);

                    // Inline play button
                    if (ImGui::SmallButton("Play"))
                    {
                        g_AudioSystem->PlayAudio(audioMeta->path);
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Stop"))
                    {
                        g_AudioSystem->Reset();
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Add to Entity"))
                    {
                        if (selectedEntity != std::numeric_limits<Entity>::max())
                        {
                            if (!ecsInterface.HasComponent<AudioComponent>(selectedEntity))
                            {
                                AudioComponent audioComp;
                                audioComp.guid = key.guid;
                                audioComp.eventName = audioMeta->name;
                                ecsInterface.AddComponent(selectedEntity, audioComp);
                                LogSuccess("Added audio component", "Audio");
                            }
                        }
                        else
                        {
                            LogWarning("Select an entity first", "Audio");
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // Handle deletions
        if (!audioToDelete.empty())
        {
            for (const auto& guid : audioToDelete)
            {
                AssetManager::Instance().Unload(guid);
            }
            AssetManager::Instance().SaveArrayToJson(TypeName<AudioMetadata>::value,
                "Assets/JSON/AudioAsset.json", "audio");
        }

        // Cleanup preview instance on tab switch
        static int lastTabIndex = -1;
        if (ImGui::GetCurrentWindow()->DC.StateStorage->GetInt(ImGui::GetID("AssetTabType")) != AssetTabType::Audio)
        {
            if (previewInstance)
            {
                previewInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                previewInstance->release();
                previewInstance = nullptr;
                isPlaying = false;
            }
        }
    }

    // Add these methods to ImguiManager.cpp for entity inspector audio components
    // These should be called from your existing entity property rendering code

    // Add this method to render AudioComponent properties in the entity inspector
    void ImguiManager::RenderAudioComponentProperties(Entity entity)
    {
        if (!ecsInterface.HasComponent<AudioComponent>(entity))
            return;

        auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);

        if (ImGui::CollapsingHeader("🔊 Audio Source", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("AudioComponent");

            // Audio clip selection combo
            if (ImGui::BeginCombo("Audio Clip", audioComp.eventName.c_str()))
            {
                for (const auto& [key, entry] : AssetManager::Instance().GetAssets())
                {
                    if (entry.typeName != TypeName<AudioMetadata>::value)
                        continue;

                    AudioMetadata* audioMeta = static_cast<AudioMetadata*>(entry.pObject.get());
                    if (audioMeta)
                    {
                        bool isSelected = (audioComp.guid == key.guid);
                        if (ImGui::Selectable(audioMeta->name.c_str(), isSelected))
                        {
                            audioComp.guid = key.guid;
                            audioComp.eventName = audioMeta->name;

                            // Stop current playback if changing clips
                            if (audioComp.eventInstance)
                            {
                                audioComp.eventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
                                audioComp.eventInstance->release();
                                audioComp.eventInstance = nullptr;
                                audioComp.isPlaying = false;
                            }
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();

            // Playback Controls Section
            ImGui::Text("Playback Controls");

            // Play/Stop/Pause buttons in a row
            if (audioComp.isPlaying)
            {
                if (ImGui::Button("⏹ Stop", ImVec2(80, 0)))
                {
                    auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                    if (audioSystem)
                    {
                        audioSystem->StopEntitySound(entity);
                    }
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Status: Playing");
            }
            else
            {
                if (ImGui::Button("▶ Play", ImVec2(80, 0)))
                {
                    auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                    if (audioSystem)
                    {
                        audioSystem->PlayEntitySound(entity);
                    }
                }
                ImGui::SameLine();
                ImGui::Text("Status: Stopped");
            }

            // Playback settings
            ImGui::Checkbox("Play On Start", &audioComp.playOnStart);
            ImGui::SameLine(200);
            ImGui::Checkbox("Loop", &audioComp.loop);

            // Volume control with visualization
            ImGui::Text("Volume");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##Volume", &audioComp.volume, 0.0f, 1.0f, "%.2f"))
            {
                if (audioComp.eventInstance)
                {
                    audioComp.eventInstance->setVolume(audioComp.volume);
                }
            }

            // Volume meter visualization
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            float height = 10.0f;

            // Background
            drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                IM_COL32(50, 50, 50, 255));

            // Volume bar
            float volumeWidth = width * audioComp.volume;
            ImU32 volumeColor = IM_COL32(
                (1.0f - audioComp.volume) * 255,
                audioComp.volume * 255,
                50, 255);
            drawList->AddRectFilled(pos, ImVec2(pos.x + volumeWidth, pos.y + height), volumeColor);

            ImGui::Dummy(ImVec2(0, height + 5));

            ImGui::Separator();

            // 3D Sound Settings Section
            ImGui::Text("3D Sound Settings");

            if (ImGui::Checkbox("Is 3D Sound", &audioComp.is3D))
            {
                // Update FMOD properties when toggling
                if (audioComp.eventInstance)
                {
                    // You may need to recreate the instance with different settings
                }
            }

            if (audioComp.is3D)
            {
                ImGui::Indent();

                // Min Distance
                ImGui::Text("Min Distance (Full Volume)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##MinDist", &audioComp.minDistance, 0.1f, 0.1f, 1000.0f, "%.1f meters"))
                {
                    // Clamp min distance
                    if (audioComp.minDistance >= audioComp.maxDistance)
                    {
                        audioComp.minDistance = audioComp.maxDistance - 0.1f;
                    }

                    if (audioComp.eventInstance)
                    {
                        audioComp.eventInstance->setProperty(
                            FMOD_STUDIO_EVENT_PROPERTY_MINIMUM_DISTANCE, audioComp.minDistance);
                    }
                }

                // Max Distance
                ImGui::Text("Max Distance (Silent)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##MaxDist", &audioComp.maxDistance, 0.5f, 1.0f, 1000.0f, "%.1f meters"))
                {
                    // Clamp max distance
                    if (audioComp.maxDistance <= audioComp.minDistance)
                    {
                        audioComp.maxDistance = audioComp.minDistance + 0.1f;
                    }

                    if (audioComp.eventInstance)
                    {
                        audioComp.eventInstance->setProperty(
                            FMOD_STUDIO_EVENT_PROPERTY_MAXIMUM_DISTANCE, audioComp.maxDistance);
                    }
                }

                // Visual Range Preview
                if (ImGui::TreeNode("Audio Range Preview"))
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImVec2 canvasSize(250, 250);

                    // Draw background
                    drawList->AddRectFilled(canvasPos,
                        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                        IM_COL32(30, 30, 30, 255));

                    // Draw grid
                    for (int i = 0; i <= 10; i++)
                    {
                        float t = i / 10.0f;
                        float x = canvasPos.x + t * canvasSize.x;
                        float y = canvasPos.y + t * canvasSize.y;
                        ImU32 gridColor = (i == 5) ? IM_COL32(80, 80, 80, 255) : IM_COL32(50, 50, 50, 255);

                        drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), gridColor);
                        drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y), gridColor);
                    }

                    // Center point (audio source)
                    ImVec2 center(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

                    // Scale factor for visualization
                    float maxRange = std::max(audioComp.maxDistance * 1.2f, 10.0f);
                    float scale = (canvasSize.x * 0.45f) / maxRange;

                    // Draw max distance circle (red)
                    drawList->AddCircle(center, audioComp.maxDistance * scale,
                        IM_COL32(255, 50, 50, 150), 64, 2.0f);

                    // Draw falloff gradient circles
                    int gradientSteps = 5;
                    for (int i = 0; i < gradientSteps; i++)
                    {
                        float t = (float)i / gradientSteps;
                        float radius = audioComp.minDistance + (audioComp.maxDistance - audioComp.minDistance) * t;
                        int alpha = 100 - (int)(t * 80);
                        drawList->AddCircle(center, radius * scale,
                            IM_COL32(100, 200, 100, alpha), 32);
                    }

                    // Draw min distance circle (green)
                    drawList->AddCircle(center, audioComp.minDistance * scale,
                        IM_COL32(50, 255, 50, 200), 48, 2.0f);

                    // Draw audio source (yellow dot)
                    drawList->AddCircleFilled(center, 5, IM_COL32(255, 255, 0, 255));
                    drawList->AddCircle(center, 5, IM_COL32(255, 200, 0, 255), 12, 2.0f);

                    // Draw listener position if available
                    if (ecsInterface.HasComponent<TransformComponent>(entity))
                    {
                        // You could show relative listener position here if needed
                    }

                    ImGui::Dummy(canvasSize);

                    // Labels
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "● Min Distance: %.1fm (Full Volume)",
                        audioComp.minDistance);
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "● Max Distance: %.1fm (Silent)",
                        audioComp.maxDistance);
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                        "Falloff: Linear | Range: %.1fm", audioComp.maxDistance - audioComp.minDistance);

                    ImGui::TreePop();
                }

                ImGui::Unindent();
            }

            ImGui::Separator();

            // Debug Information
            if (ImGui::TreeNode("Debug Info"))
            {
                ImGui::Text("GUID: %s", audioComp.guid.id.c_str());
                ImGui::Text("Event Name: %s", audioComp.eventName.c_str());
                ImGui::Text("Is Playing: %s", audioComp.isPlaying ? "Yes" : "No");
                ImGui::Text("Channel ID: %d", audioComp.channelID);
                ImGui::Text("Event Instance: %p", audioComp.eventInstance);
                ImGui::Text("Last Position: (%.2f, %.2f, %.2f)",
                    audioComp.lastPosition.x, audioComp.lastPosition.y, audioComp.lastPosition.z);

                // Real-time distance to listener (if available)
                auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                if (audioSystem && ecsInterface.HasComponent<TransformComponent>(entity))
                {
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
                    // You'd need to expose listener position from AudioSystem
                    // float distance = glm::length(transform.position - listenerPosition);
                    // ImGui::Text("Distance to Listener: %.2fm", distance);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    // Add this method to render ListenerComponent properties in the entity inspector
    void ImguiManager::RenderListenerComponentProperties(Entity entity)
    {
        if (!ecsInterface.HasComponent<ListenerComponent>(entity))
            return;

        auto& listener = ecsInterface.GetComponent<ListenerComponent>(entity);

        if (ImGui::CollapsingHeader("🎧 Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("ListenerComponent");

            // Active state
            if (ImGui::Checkbox("Active", &listener.active))
            {
                // Update audio system when toggling
                auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                if (audioSystem && listener.active)
                {
                    audioSystem->SetListenerPosition(listener.position, listener.forward, listener.up);
                }
            }

            if (!listener.active)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "⚠ Listener is inactive - no 3D audio will be heard");
            }

            ImGui::Text("Listener Index: %d", listener.listenerIndex);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Index for multi-listener support (e.g., split-screen)");
            }

            ImGui::Separator();

            // Transform Section
            ImGui::Text("Transform");

            // Position
            if (ImGui::DragFloat3("Position", &listener.position.x, 0.1f))
            {
                if (listener.active)
                {
                    auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                    if (audioSystem)
                    {
                        audioSystem->SetListenerPosition(listener.position, listener.forward, listener.up);
                    }
                }
            }

            // Orientation
            ImGui::Text("Orientation");
            ImGui::Indent();

            if (ImGui::DragFloat3("Forward", &listener.forward.x, 0.01f, -1.0f, 1.0f))
            {
                // Keep normalized
                listener.forward = glm::normalize(listener.forward);
                if (listener.active)
                {
                    auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                    if (audioSystem)
                    {
                        audioSystem->SetListenerPosition(listener.position, listener.forward, listener.up);
                    }
                }
            }

            if (ImGui::DragFloat3("Up", &listener.up.x, 0.01f, -1.0f, 1.0f))
            {
                // Keep normalized
                listener.up = glm::normalize(listener.up);
                if (listener.active)
                {
                    auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                    if (audioSystem)
                    {
                        audioSystem->SetListenerPosition(listener.position, listener.forward, listener.up);
                    }
                }
            }

            if (ImGui::Button("Normalize Orientation"))
            {
                listener.forward = glm::normalize(listener.forward);
                listener.up = glm::normalize(listener.up);

                // Ensure orthogonal
                glm::vec3 right = glm::normalize(glm::cross(listener.forward, listener.up));
                listener.up = glm::normalize(glm::cross(right, listener.forward));
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset to Default"))
            {
                listener.forward = glm::vec3(0.0f, 0.0f, -1.0f);
                listener.up = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            ImGui::Unindent();

            ImGui::Separator();

            // Physics Section (for Doppler effect)
            ImGui::Text("Physics");
            ImGui::DragFloat3("Velocity", &listener.velocity.x, 0.1f);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Velocity for Doppler effect calculation");
            }

            // Velocity magnitude
            float speed = glm::length(listener.velocity);
            ImGui::Text("Speed: %.2f m/s", speed);

            ImGui::Separator();

            // Visualization
            if (ImGui::TreeNode("Listener Orientation Preview"))
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImVec2 canvasSize(200, 200);

                // Draw background
                drawList->AddRectFilled(canvasPos,
                    ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                    IM_COL32(30, 30, 30, 255));

                // Center point
                ImVec2 center(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

                // Draw axes
                float axisLength = 80.0f;

                // Forward vector (blue arrow)
                ImVec2 forwardEnd(
                    center.x + listener.forward.x * axisLength,
                    center.y - listener.forward.z * axisLength  // Flip Z for 2D view
                );
                drawList->AddLine(center, forwardEnd, IM_COL32(100, 150, 255, 255), 3.0f);
                drawList->AddTriangleFilled(
                    forwardEnd,
                    ImVec2(forwardEnd.x - 5, forwardEnd.y - 8),
                    ImVec2(forwardEnd.x + 5, forwardEnd.y - 8),
                    IM_COL32(100, 150, 255, 255)
                );

                // Up vector (green arrow)
                ImVec2 upEnd(
                    center.x + listener.up.x * axisLength * 0.6f,
                    center.y - listener.up.y * axisLength * 0.6f
                );
                drawList->AddLine(center, upEnd, IM_COL32(100, 255, 100, 255), 2.0f);

                // Listener head (circle)
                drawList->AddCircleFilled(center, 10, IM_COL32(255, 200, 100, 255));

                // Draw "ears" (semicircles)
                drawList->AddCircle(ImVec2(center.x - 12, center.y), 5, IM_COL32(255, 255, 255, 255), 12, 2.0f);
                drawList->AddCircle(ImVec2(center.x + 12, center.y), 5, IM_COL32(255, 255, 255, 255), 12, 2.0f);

                ImGui::Dummy(canvasSize);

                // Legend
                ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "→ Forward");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "↑ Up");

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    void ImguiManager::RenderAnimationAssets()
    {
        // --- Help Tooltip ---
        ImGui::TextColored(helpYellow, "(❓)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(
                "- The Animation panel lists all registered animation assets (FBX, GLTF, etc.).\n"
                "- Right-click the icon to rename or delete the asset.\n"
                "- Deleting an asset unloads it and removes it from the Asset Manager.\n\n"
                "- Hovering shows animation details such as file path and GUID."
            );
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // --- Grid/Column Setup ---
        float windowWidth = ImGui::GetContentRegionAvail().x;
        float itemWidth = 80.0f; // Width of each item
        int columns = std::max(1, static_cast<int>(windowWidth / itemWidth));

        std::vector<std::string> animsToDelete;
        const auto& assets = AssetManager::Instance().GetAssets();
        int count = 0;

        static char nameBuffer[256] = {}; // Static buffer for renaming

        // --- Displaying each asset ---
        for (const auto& [guid, entry] : assets)
        {
            // Filter for AnimationProperties
            if (entry.typeName != TypeName<AnimationProperties>::value)
                continue;

            auto* anim = static_cast<AnimationProperties*>(entry.pObject.get());
            if (!anim || anim->guid.empty())
                continue;

            std::string modalId = "EditAnimModal_" + anim->guid;

            ImGui::PushID(anim->guid.id.c_str());
            ImGui::BeginGroup();

            // --- Centered placeholder icon (using g_ObjIcon) ---
            float imageSize = 64.0f;
            float imageX = (itemWidth - imageSize) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + imageX);
            ImGui::Image((ImTextureID)g_ObjIcon.textureID, ImVec2(imageSize, imageSize));

            // --- Right-click on image to open modal ---
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                // Initialize rename buffer with current name
                std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", anim->name.c_str());
                ImGui::OpenPopup(modalId.c_str());
            }

            // --- Centered asset name ---
            const int maxChars = 10;
            std::string displayName = anim->name;
            if (displayName.length() > maxChars)
                displayName = displayName.substr(0, maxChars - 3) + "...";

            float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
            float centerOffset = (itemWidth - textWidth) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centerOffset);

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + itemWidth);
            ImGui::TextWrapped("%s", displayName.c_str());
            ImGui::PopTextWrapPos();

            // --- Modal Popup Logic ---
            if (ImGui::BeginPopupModal(modalId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Animation Options");
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::Text("Edit Name:");
                ImGui::InputText("##Name", nameBuffer, IM_ARRAYSIZE(nameBuffer));

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
                    if (newName != anim->name)
                    {
                        anim->name = newName;
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine(0, spacing);
                if (ImGui::Button("Delete", ImVec2(buttonWidth, 0)))
                {
                    animsToDelete.push_back(anim->guid.id.c_str());

                    // Delete the actual .fbx file
                    if (!AssetManager::Instance().DeleteAssetFile(anim->path))
                    {
                        errorMessage = "Failed to delete animation file:\n" + anim->path;
                        showErrorPopup = true;
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine(0, spacing);
                if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            // --- Tooltip on hover ---
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Name: %s", anim->name.c_str());
                ImGui::Text("Path: %s", anim->path.c_str());
                ImGui::Text("GUID: %s", anim->guid.id.c_str());
                ImGui::Text("Bones: %zu", anim->skeleton.bones.size());
                ImGui::Text("Clips: %zu", anim->availableAnimations.size());
                ImGui::EndTooltip();
            }

            ImGui::EndGroup();
            ImGui::PopID();

            count++;
            if (count % columns != 0)
                ImGui::SameLine();
        }

        // --- Deletion Logic ---
        if (!animsToDelete.empty())
        {
            for (const auto& guid : animsToDelete)
            {
                AssetManager::Instance().Unload(guid); // Unload from memory
            }
            // Save the updated JSON file
            AssetManager::Instance().SaveArrayToJson(
                TypeName<AnimationProperties>::value,
                "Assets/JSON/AnimationAsset.json",
                "animations" // This MUST match your CompileArray key
            );
        }
        ImGui::NewLine();
        RenderErrorPopup(); // Show popup if file deletion failed
    }
    
    void ImguiManager::RenderPrefabAssets()
    {
        const auto& assets = AssetManager::Instance().GetAssets();
        for (const auto& [guid, entry] : assets)
        {
            if (entry.typeName != TypeName<PrefabProperties>::value)
                continue;

            const auto* prefab = static_cast<const PrefabProperties*>(entry.pObject.get());
            if (!prefab) continue;

            ImGui::BeginGroup();
            ImGui::Text(prefab->name.c_str());

            // Start drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("PREFAB_GUID", prefab->guid.id.c_str(), prefab->guid.id.size() + 1);
                ImGui::Text("Dragging: %s", prefab->name.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::EndGroup();
        }
    }

    void ImguiManager::RenderSceneAssets()
    {
        const auto& assets = AssetManager::Instance().GetAssets();
        for (const auto& [guid, entry] : assets)
        {
            if (entry.typeName != TypeName<SceneProperties>::value)
                continue;

            const auto* scene = static_cast<const SceneProperties*>(entry.pObject.get());
            if (!scene) continue;

            ImGui::BeginGroup();
            ImGui::Text(scene->name.c_str());

            // Drag-and-drop support
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("SCENE_GUID", scene->guid.id.c_str(), scene->guid.id.size() + 1);
                ImGui::Text("Dragging: %s", scene->name.c_str());
                ImGui::EndDragDropSource();
            }

            // Optional: double-click to open scene
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                sceneManager.LoadScene(scene->path); // Your scene loading logic
            }
            ImGui::EndGroup();
        }
    }

    void ImguiManager::RenderScriptAssets()
    {
        const auto& assets = AssetManager::Instance().GetAssets();
        for (const auto& [guid, entry] : assets)
        {
            if (entry.typeName != TypeName<ScriptEntry>::value) // <-- use your ScriptAsset type
                continue;

            const auto* script = static_cast<const ScriptEntry*>(entry.pObject.get());
            if (!script) continue;

            ImGui::BeginGroup();
            ImGui::Text("%s", script->scriptPath.c_str());

            // Drag-and-drop support
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                // Send GUID as payload
                ImGui::SetDragDropPayload("SCRIPT_GUID",
                    script->guid.id.c_str(),
                    script->guid.id.size() + 1);
                ImGui::Text("Dragging: %s", script->scriptPath.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::EndGroup();
        }
    }

    int ImguiManager::GetMainViewIndex() const { return g_MainViewIndex; }

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
                    ecsInterface.AddComponent<HierarchyComponent>(newEntity, HierarchyComponent{}); // Default hierarchy

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

                    // Automate Generate Entity Inspector
                    DrawEntityInspector(selectedEntity, ecsInterface);

                    // Reset selectedEntity if the properties window is closed
                    if (!isPropertiesWindowOpen)
                    {
                        selectedEntity = std::numeric_limits<Entity>::max();
                    }
                    ImGui::End();
                }
            }
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

				//to track if bgm was paused
                static bool bgmWasPaused = false;

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

					//Send message to start audio and other play-mode systems
                    MessageBus::Get().SendImmediate(EnterPlayModeMessage());

                    //if (g_AudioSystem) {
                    //    std::string bgmPath = sceneManager.GetCurrentSceneBGMPath();
                    //    if (!bgmPath.empty()) {
                    //        if (bgmWasPaused) {
                    //            // Just resume the paused audio
                    //            g_AudioSystem->ResumeAllAudio();
                    //            std::cout << "[Audio] Resuming BGM: " << bgmPath << std::endl;
                    //            bgmWasPaused = false;
                    //        }
                    //        else {
                    //            // Start fresh BGM
                    //            g_AudioSystem->PlayAudio(bgmPath);
                    //            std::cout << "[Audio] Playing BGM: " << bgmPath << std::endl;
                    //        }
                    //    }
                    //}
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
                        
                        //Send message to pause/stop audio
						//MessageBus::Get().SendImmediate(ExitPlayModeMessage());

                        //Pause BGM 
                        /*if (g_AudioSystem) {
							g_AudioSystem->PauseAllAudio();
                            bgmWasPaused = true;
							std::cout << "[Audio] BGM Paused" << std::endl;
                        }*/
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

					//Send message to stop all audio
					MessageBus::Get().SendImmediate(ExitPlayModeMessage());

                    //Stop BGM immediately 
                    /*if (g_AudioSystem) {
                        g_AudioSystem->Reset();
                        bgmWasPaused = false;
                        std::cout << "[Audio] BGM Stopped" << std::endl;
                    }*/
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
                    Framework::Graphics::s_RenderDebugColliders = !Framework::Graphics::s_RenderDebugColliders;
                }
                ImGui::PopStyleColor(3); // Restore color
            }
            ImGui::End();
            bool menuActive = !engineState.IsPlay();
            if (ImGui::Begin("Main Viewport"))
            {
                // Get the available size in the ImGui window
                ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                float aspectRatio = (m_fboH > 0) ? (float)m_fboW / (float)m_fboH : 16.0f / 9.0f;
                float newWidth = viewportSize.x;
                float newHeight = newWidth / aspectRatio;

                // Adjust width and height to maintain the aspect ratio
                if (newHeight > viewportSize.y)
                {
                    newHeight = viewportSize.y;
                    newWidth = newHeight * aspectRatio;
                }

                // Center the image in the available viewport space with black bars
                ImVec2 offset((viewportSize.x - newWidth) * 0.5f, (viewportSize.y - newHeight) * 0.5f);
                ImGui::SetCursorPos(offset);

                // Render the framebuffer texture within the calculated size
                //int mainView = 0;
                ImGui::Image(
                    (ImTextureID)(intptr_t)GetGameTexture(g_MainViewIndex),
                    ImVec2(newWidth, newHeight), ImVec2(0, 1), ImVec2(1, 0)
                );

                // jay handle mouse click and get UV coordinates
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_graphicsSystem->SetMouseTargetView(g_MainViewIndex);
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImVec2 pos = ImGui::GetIO().MousePos;

                    float u = (pos.x - min.x) / (max.x - min.x);
                    float v = (pos.y - min.y) / (max.y - min.y);
                    v = 1.0f - v;

                    //if (!menuActive) {
                        // hand normalized UV to Graphics for the readback
                        m_graphicsSystem->SetPickViewIndex(g_MainViewIndex); // assuming view index 0 for main viewport
                        m_graphicsSystem->SetGroundMode(true);
                        if (g_MainViewIndex == 0) {
                            m_graphicsSystem->RequestPickUV(u, v);

                        }
                        else {
                            m_graphicsSystem->RequestPickUVSecondary(u, v);
                        }
                    //}
                    // NEW: forward the same UV to ButtonSystem for 2D button hit-test
                    auto buttonSys = ecsInterface.GetSystem<ButtonSystem>();
                    if (buttonSys) {
                        buttonSys->HandleClickUV(u, v);
                    }

                }
            }
            ImGui::End();

            // Debug second viewport window
            if (ImGui::Begin("Secondary Viewport"))
            {
                // --- 1. Viewport Sizing and Letterboxing ---
                ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                float aspectRatio = (m_fboH > 0) ? (float)m_fboW / (float)m_fboH : 16.0f / 9.0f;

                float newWidth = viewportSize.x;
                float newHeight = newWidth / aspectRatio;

                if (newHeight > viewportSize.y) {
                    newHeight = viewportSize.y;
                    newWidth = newHeight * aspectRatio;
                }

                // Calculate offset for centering the image
                ImVec2 offset((viewportSize.x - newWidth) * 0.5f, (viewportSize.y - newHeight) * 0.5f);
                ImGui::SetCursorPos(offset);

                // --- 2. Draw the Viewport Image ---
                int secondaryView = (g_MainViewIndex == 0) ? 1 : 0;
                ImGui::Image(
                    (ImTextureID)(intptr_t)GetGameTexture(secondaryView),
                    ImVec2(newWidth, newHeight),
                    ImVec2(0, 1), // UV Coords: Top-Left
                    ImVec2(1, 0)  // UV Coords: Bottom-Right (flips V)
                );

                // --- 3. Render Gizmo FIRST ---
                // Get the screen-space rectangle of the image we just drew
                ImVec2 rectMin = ImGui::GetItemRectMin();
                ImVec2 rectMax = ImGui::GetItemRectMax();
                ImVec2 vpSize(rectMax.x - rectMin.x, rectMax.y - rectMin.y);

                // Get the camera matrices for the secondary view
                glm::mat4 view = m_graphicsSystem->GetViewMatrix(secondaryView);
                glm::mat4 proj = m_graphicsSystem->GetProjectionMatrix(secondaryView, static_cast<float>(m_fboW), static_cast<float>(m_fboH));

                // Render the gizmo (this calls ImGuizmo::Manipulate)
                RenderGizmoForSelected(view, proj, rectMin, vpSize);

                // --- 4. Check Gizmo State ---
                // Check if the mouse is hovering or actively dragging a gizmo handle
                bool isGizmoActive = ImGuizmo::IsUsing() || ImGuizmo::IsOver();

                // --- 5. Conditional Picking Logic ---
                // Only perform entity picking if:
                // 1. The mouse is over the viewport image
                // 2. The left mouse button is clicked
                // 3. The mouse is NOT interacting with the gizmo
                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isGizmoActive)
                {
                    m_graphicsSystem->SetMouseTargetView(secondaryView);

                    // Get mouse position relative to the image
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImVec2 pos = ImGui::GetIO().MousePos;

                    // Calculate UV coordinates (0.0 to 1.0)
                    float u = (pos.x - min.x) / (max.x - min.x);
                    float v = (pos.y - min.y) / (max.y - min.y);
                    v = 1.0f - v; // Flip V-coordinate to match OpenGL/texture convention

                    // Request the pick
                    //if (!menuActive) {
                        m_graphicsSystem->SetPickViewIndex(secondaryView);
                        m_graphicsSystem->SetGroundMode(true);
                        if (secondaryView == 0)
                            m_graphicsSystem->RequestPickUV(u, v);
                        else
                            m_graphicsSystem->RequestPickUVSecondary(u, v);
                    //}
                    auto buttonSys = ecsInterface.GetSystem<ButtonSystem>();
                    if (buttonSys) {
                        buttonSys->HandleClickUV(u, v);
                    }
                }

                // --- 6. Conditional Selection Logic ---
                // Check if the graphics system has a new picked ID for us
                if (int id = m_graphicsSystem->ConsumeLastPickedId(); id >= 0)
                {
                    // IMPORTANT: Only change the selected entity if the user is
                    // NOT in the middle of dragging the gizmo.
                    if (!ImGuizmo::IsUsing())
                    {
                        selectedEntity = static_cast<Entity>(id);
                        isPropertiesWindowOpen = true; // Optional: auto-open properties
                    }
                }
            }

            // Drag and drop Prefab to spawn in viewport
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB_GUID"))
                {
                    const char* prefabGUID = static_cast<const char*>(payload->Data);

                    // Spawn the prefab
                    sceneManager.SpawnPrefab(prefabGUID);
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::End();


        }
    }

    void ImguiManager::RenderGizmoPanel()
    {
        if (ImGui::Begin("Gizmos", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::Text("Gizmo Controls");
            ImGui::Separator();

            ImGui::Checkbox("Enabled", &m_gizmoEnabled);

            ImGui::Text("Operation");
            ImGui::RadioButton("Translate", &m_gizmoOperation, 0); ImGui::SameLine();
            ImGui::RadioButton("Rotate", &m_gizmoOperation, 1); ImGui::SameLine();
            ImGui::RadioButton("Scale", &m_gizmoOperation, 2);

            ImGui::Text("Space");
            ImGui::RadioButton("Local", &m_gizmoMode, 0); ImGui::SameLine();
            ImGui::RadioButton("World", &m_gizmoMode, 1);

        }
        ImGui::End();
    }

    void ImguiManager::RenderGizmoForSelected(const glm::mat4& view, const glm::mat4& proj,
        const ImVec2& viewportTopLeft, const ImVec2& viewportSize)
    {
        if (!m_gizmoEnabled) return;

        extern Framework::Coordinator ecsInterface;
        if (selectedEntity == std::numeric_limits<Entity>::max()) return;
        if (!ecsInterface.HasComponent<TransformComponent>(selectedEntity)) return;

        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);

        auto& tr = ecsInterface.GetComponent<TransformComponent>(selectedEntity);

        // Build model from your stored TRS (rotation is in degrees)
        glm::mat4 model(1.0f);
        model = glm::translate(model, tr.position);
        model = glm::rotate(model, glm::radians(tr.rotation.x), { 1,0,0 });
        model = glm::rotate(model, glm::radians(tr.rotation.y), { 0,1,0 });
        model = glm::rotate(model, glm::radians(tr.rotation.z), { 0,0,1 });
        model = glm::scale(model, tr.scale);

        ImGuizmo::OPERATION op = (m_gizmoOperation == 0) ? ImGuizmo::TRANSLATE :
            (m_gizmoOperation == 1) ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
        ImGuizmo::MODE mode = (m_gizmoMode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        // (optional) snapping
        static bool  useSnap = false;
        static float snapT[3] = { 0.1f,0.1f,0.1f };
        static float snapR = 5.0f;   // degrees
        static float snapS[3] = { 0.1f,0.1f,0.1f };
        float* snap = nullptr;
        if (useSnap) snap = (op == ImGuizmo::TRANSLATE) ? snapT : (op == ImGuizmo::ROTATE) ? &snapR : snapS;

        // Use delta for stable rotation updates
        glm::mat4 delta(1.0f);
        if (ImGuizmo::Manipulate(glm::value_ptr(view),
            glm::value_ptr(proj),
            op, mode,
            glm::value_ptr(model),
            glm::value_ptr(delta),
            snap))
        {
            if (op == ImGuizmo::ROTATE)
            {
                // Apply only the incremental rotation (degrees) to avoid Euler flip/jitter
                float dt[3], dr[3], ds[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(delta), dt, dr, ds);

                tr.rotation += glm::vec3(dr[0], dr[1], dr[2]); // add delta Euler (deg)

                // keep angles tidy
                auto wrap = [](float a) {
                    if (a > 180.f) a = fmodf(a + 180.f, 360.f) - 180.f;
                    if (a < -180.f) a = fmodf(a - 180.f, 360.f) + 180.f;
                    return a;
                    };
                tr.rotation.x = wrap(tr.rotation.x);
                tr.rotation.y = wrap(tr.rotation.y);
                tr.rotation.z = wrap(tr.rotation.z);
            }
            else
            {
                // For translate/scale, full decompose is fine
                float t[3], r[3], s[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, r, s);
                tr.position = { t[0], t[1], t[2] };
                tr.rotation = { r[0], r[1], r[2] };  // degrees
                tr.scale = { s[0], s[1], s[2] };
            }
            tr.isDirty = true;
        }
    }
}