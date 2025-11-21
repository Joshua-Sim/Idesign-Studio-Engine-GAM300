#include "pch.h"
#include "ScriptingSystem.h"
#include "Coordinator.h"
//#include "ComponentList.h"
#include "Graphics.h"          // to call Graphics* methods
#include <lua.hpp>
#include <iostream>
#include "AssetCompiler.h"
#include "WindowConfigProperties.h"  // g_WindowConfigProps
#include "AudioLoader.h"             // g_AudioPropertiesProps
#include "TextureLoader.h"           // g_TexturePropertiesProps
#include "NavMeshSystem.h"
#include "MeshProperties.h"          // g_MeshPropertiesProps
#include <filesystem>
#include "ScriptProperties.h"

extern Framework::Coordinator ecsInterface;

namespace Framework {

    // -------- file-scope helpers (must NOT be inside a member function) --------
    namespace {
        // Where we store/retrieve the ScriptingSystem* inside Lua's registry
        constexpr const char* kRegistryKey = "__ScriptingSystemPtr";

        inline ScriptingSystem* lua_self(lua_State* L) {
            lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey);
            auto* self = static_cast<ScriptingSystem*>(lua_touserdata(L, -1));
            lua_pop(L, 1);
            return self;
        }

        static int lua_GetPosition(lua_State* L)
        {
            // 1st arg: entity id
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            if (!ecsInterface.IsEntityValid(e) ||
                !ecsInterface.HasComponent<TransformComponent>(e))
            {
                // no transform; return 0 values
                return luaL_error(L, "GetPosition: entity %d has no TransformComponent", (int)e);
            }

            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);

            lua_pushnumber(L, tr.position.x);
            lua_pushnumber(L, tr.position.y);
            lua_pushnumber(L, tr.position.z);
            return 3; // x, y, z
        }

        static int lua_SetPosition(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            float x = static_cast<float>(luaL_checknumber(L, 2));
            float y = static_cast<float>(luaL_checknumber(L, 3));
            float z = static_cast<float>(luaL_checknumber(L, 4));

            if (!ecsInterface.IsEntityValid(e) ||
                !ecsInterface.HasComponent<TransformComponent>(e))
            {
                return luaL_error(L, "SetPosition: entity %d has no TransformComponent", (int)e);
            }

            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);
            tr.position = glm::vec3(x, y, z);
            return 0;
        }

        int l_IsKeyDown(lua_State* L) {
            auto* self = lua_self(L);
            Graphics* g = self ? self->GetGraphicsSystem() : nullptr;     // <-- use getter
            if (!g) { lua_pushboolean(L, 0); return 1; }
            GLFWwindow* win = g->GetWindow();
            int key = (int)luaL_checkinteger(L, 1);
            int down = (win && glfwGetKey(win, key) == GLFW_PRESS) ? 1 : 0;
            lua_pushboolean(L, down);
            return 1;
        }

        int l_CameraMove(lua_State* L) {
            auto* self = lua_self(L);
            Graphics* g = self ? self->GetGraphicsSystem() : nullptr;     // <-- use getter
            if (!g) return 0;
            const char* dir = luaL_checkstring(L, 1);
            float dt = (float)luaL_checknumber(L, 2);
            bool gm = (lua_gettop(L) >= 3) ? (lua_toboolean(L, 3) != 0) : g->GetGroundModeFromLua();
            g->MoveCameraFromLua(dir, dt, gm);
            return 0;
        }

        int l_NudgeLight(lua_State* L) {
            auto* self = lua_self(L);
            Graphics* g = self ? self->GetGraphicsSystem() : nullptr;
            if (!g) return 0;
            const char* axis = luaL_checkstring(L, 1);
            float amt = (float)luaL_checknumber(L, 2);
            g->NudgeLightFromLua(axis, amt);
            return 0;
        }

        // In ScriptingSystem.cpp, inside the anonymous namespace or static helpers

        static int l_spawnPrompt(lua_State* L) {
            // Argument 1: The Entity ID (The Box/Receiver) - so we know WHERE to spawn
            Entity parentID = static_cast<Entity>(luaL_checkinteger(L, 1));

            // Argument 2: The Text to display
            const char* textContent = luaL_checkstring(L, 2);

            // Argument 3: (Optional) Y Offset
            float yOffset = (float)luaL_optnumber(L, 3, 2.0); // Default to 2.0 units high

            // 1. Safety Check
            if (!ecsInterface.IsEntityValid(parentID) ||
                !ecsInterface.HasComponent<TransformComponent>(parentID)) {
                std::cout << "[Lua Error] SpawnPrompt: Invalid entity or missing transform.\n";
                return 0;
            }

            // 2. Get Parent Position
            auto& parentTrans = ecsInterface.GetComponent<TransformComponent>(parentID);
            glm::vec3 spawnPos = parentTrans.position;
            spawnPos.y += yOffset;

            // 3. Create New Entity
            Entity promptEntity = ecsInterface.CreateEntity();

            // 4. Add Transform
            TransformComponent tf;
            tf.position = spawnPos;
            tf.scale = glm::vec3(1.0f);
            tf.rotation = glm::vec3(0.0f, 90.0f, 0.0f);
            ecsInterface.AddComponent(promptEntity, tf);

            HierarchyComponent hc;
            ecsInterface.AddComponent(promptEntity, hc);

            TextProperties tp;
            tp.text = std::string(textContent);
            tp.color = glm::vec3(1.0f, 1.0f, 1.0f); // White text
            tp.alignment = 1;
            tp.scale = 0.03f;
            ecsInterface.AddComponent(promptEntity, tp);

            MeshProperties mp;
            ecsInterface.AddComponent(promptEntity, mp);

            MaterialProperties mp2;
            ecsInterface.AddComponent(promptEntity, mp2);

            // Optional: Return the new ID to Lua so we can delete it later?
            lua_pushinteger(L, static_cast<lua_Integer>(promptEntity));
            return 1;
        }

        static int l_DestroyEntity(lua_State* L) {
            // 1. Get Entity ID from Lua
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            // 2. Check validity
            if (ecsInterface.IsEntityValid(e)) {
                ecsInterface.DestroyEntity(e);
                // std::cout << "[Lua] Destroyed Entity " << e << std::endl;
            }
            else {
                std::cout << "[Lua Warning] Tried to destroy invalid entity: " << e << std::endl;
            }
            return 0;
        }

        int l_ToggleLight(lua_State* L) {
            auto* self = lua_self(L);
            Graphics* g = self ? self->GetGraphicsSystem() : nullptr;
            if (!g) return 0;
            g->ToggleMainLightFromLua();
            return 0;
        }

        int l_SetHDR(lua_State* L) { auto* s = lua_self(L); auto* g = s ? s->GetGraphicsSystem() : nullptr; if (g) g->SetHDRFromLua((float)luaL_checknumber(L, 1)); return 0; }
        int l_SetBloom(lua_State* L) { auto* s = lua_self(L); auto* g = s ? s->GetGraphicsSystem() : nullptr; if (g) g->SetBloomFromLua((float)luaL_checknumber(L, 1)); return 0; }
        int l_SetGround(lua_State* L) { auto* s = lua_self(L); auto* g = s ? s->GetGraphicsSystem() : nullptr; if (g) g->SetGroundModeFromLua(lua_toboolean(L, 1) != 0);   return 0; }

        // Engine LUA exposed utility functions (Get entity, get by component, get by name, tag , create prefab, setactive -deactive etc

        
        int lua_FindEntityByName(lua_State* L)
        {
            const char* name = luaL_checkstring(L, 1);
            Entity e = ecsInterface.GetEntityByName(name);
            if (ecsInterface.IsEntityValid(e)) {
                lua_pushinteger(L, static_cast<int>(e));
            }
            else {
                lua_pushnil(L);
            }
            return 1;
        }

        static int lua_SetMesh(lua_State* L)
        {
            // 1. Get arguments: Entity ID and Mesh Path
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* path = luaL_checkstring(L, 2);

            // 2. Validate Entity & Component
            if (ecsInterface.IsEntityValid(e) && ecsInterface.HasComponent<MeshProperties>(e)) {
                auto& mesh = ecsInterface.GetComponent<MeshProperties>(e);

                // 3. Update Path
                mesh.path = path;

                // 4. Reload Mesh Data Immediately
                MeshLoader::loadMesh(mesh.path, mesh);
            }
            return 0;
        }

        static int lua_SetTexture(lua_State* L)
        {
            // 1. Get arguments: Entity ID and Texture Path
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* path = luaL_checkstring(L, 2);

            // 2. Validate Entity & Component
            if (ecsInterface.IsEntityValid(e) && ecsInterface.HasComponent<TextureProperties>(e)) {
                auto& tex = ecsInterface.GetComponent<TextureProperties>(e);

                // 3. Update Path
                tex.path = path;

                // 4. Reload Texture Data Immediately
                TextureLoader::loadTexture(tex, true);
            }
            return 0;
        }
        // NavMesh Exposed funcs for path-finding

        void ScriptPathfindTo(const std::string& agentName, const std::string& targetName)
        {
            auto& ecs = ecsInterface;
            auto navSystem = ecs.GetSystem<Framework::NavMeshSystem>();
            if (!navSystem)
                return;

            Entity agent = ecs.GetEntityByName(agentName);
            Entity target = ecs.GetEntityByName(targetName);

            if (!ecs.IsEntityValid(agent) || !ecs.IsEntityValid(target))
            {
                std::cerr << "[Lua] Invalid agent or target name!" << std::endl;
                return;
            }

            bool missing = false;

            if (!ecs.HasComponent<NavMeshAgentComponent>(agent)) {
                std::cerr << "[Lua] Missing NavMeshAgentComponent on agent!" << std::endl;
                missing = true;
            }

            if (!ecs.HasComponent<TransformComponent>(agent)) {
                std::cerr << "[Lua] Missing TransformComponent on agent!" << std::endl;
                missing = true;
            }

            if (!ecs.HasComponent<TransformComponent>(target)) {
                std::cerr << "[Lua] Missing TransformComponent on target!" << std::endl;
                missing = true;
            }

            if (missing) {
                return;
            }


            auto& agentXform = ecs.GetComponent<TransformComponent>(agent);
            auto& targetXform = ecs.GetComponent<TransformComponent>(target);

            glm::vec3 from = navSystem->GetNearestPointOnNavmesh(agentXform.position);
            glm::vec3 to = navSystem->GetNearestPointOnNavmesh(targetXform.position);

            std::cout << "[Lua] Pathfinding from '" << agentName << "' to '" << targetName << "'..." << std::endl;

            navSystem->SetAgentDestination(agent, to);
        }


        namespace fs = std::filesystem;

        // Join a relative path with fallback prefixes until a file is found.
        static std::string resolve_path(const std::string& rel) {
            // First try from project root
            fs::path fromRoot = fs::path(ScriptingSystem::GetProjectRoot()) / rel;
            std::error_code ec;
            if (fs::exists(fromRoot, ec)) {
                return fs::weakly_canonical(fromRoot, ec).string();
            }

            // Fallback to old behavior for backwards compatibility
            const char* roots[] = { ".", "..", "../.." };
            for (auto r : roots) {
                fs::path p = fs::path(r) / rel;
                if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec).string();
            }

            // If not found, return as-is
            return rel;
        }

        static bool prepare_output_and_skip_if_fresh(const std::string& jsonPath,
            const std::string& outPath) {
            namespace fs = std::filesystem;
            try {
                fs::path outp(outPath);
                if (!outp.parent_path().empty())
                    fs::create_directories(outp.parent_path());

                if (fs::exists(outPath)) {
                    auto tJson = fs::last_write_time(jsonPath);
                    auto tBin = fs::last_write_time(outPath);
                    if (tBin >= tJson) return true; // already up-to-date
                }
            }
            catch (...) {
                // swallow; we'll just recompile
                std::cerr << "[Cook] Warning: failed to check timestamps — forcing recompile.\n";
            }
            return false;
        }

        // Cook.WindowConfig(jsonPath, outPath)
        int l_Cook_WindowConfig(lua_State* L) {
            std::string json = resolve_path(luaL_checkstring(L, 1));
            std::string out = resolve_path(luaL_checkstring(L, 2));
            std::string desc = resolve_path(luaL_checkstring(L, 3));

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileSingleObject<WindowConfig>(json, out, desc, g_WindowConfigProps);
            lua_pushboolean(L, ok);
            return 1;
        }

        // This function implements the "IsMouseButtonDown(button)" for Lua
        static int l_IsMouseButtonDown(lua_State* L) {
            // Get the 'self' object (ScriptingSystem or ScriptInstance)
            auto* self = lua_self(L);
            Graphics* g = self ? self->GetGraphicsSystem() : nullptr;
            if (!g) {
                lua_pushboolean(L, 0);
                return 1; // Return 1 value (the boolean false)
            }
            GLFWwindow* win = g->GetWindow();
            int button = (int)luaL_checkinteger(L, 1);
            int down = (win && glfwGetMouseButton(win, button) == GLFW_PRESS) ? 1 : 0;
            lua_pushboolean(L, down);
            return 1;
        }

        // Cook.Audio(jsonPath, outPath [, key="AudioProperties"])
        int l_Cook_Audio(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "AudioProperties");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<AudioMetadata>(json, out, desc, g_AudioMetadataProps, key);
            lua_pushboolean(L, ok);
            return 1;
        }

        // Cook.Textures(jsonPath, outPath [, key="textures"])
        int l_Cook_Textures(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "textures");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<TextureProperties>(json, out, desc, g_TexturePropertiesProps, key);
            lua_pushboolean(L, ok);
            return 1;
        }

        // Cook.Meshes(jsonPath, outPath [, key="mesh"])
        int l_Cook_Meshes(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "mesh");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<MeshProperties>(json, out, desc, g_MeshProps, key);
            lua_pushboolean(L, ok);
            return 1;
        }

        // Cook.Prefab(jsonPath, outPath [, key="prefabs"])
        int l_Cook_Prefab(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "prefabs");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<PrefabProperties>(json, out, desc, g_PrefabProps, key);
            lua_pushboolean(L, ok);
            return 1;
        }

        // Cook.Scene(jsonPath, outPath [, key="scenes"])
        int l_Cook_Scene(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "scenes");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<SceneProperties>(json, out, desc, g_SceneProps, key);
            lua_pushboolean(L, ok);
            return 1;
        }
    
        // Cook.Scene(jsonPath, outPath [, key="materials"])
        int l_Cook_Material(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "materials");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<MaterialProperties>(json, out, desc, g_MaterialProps, key);
            lua_pushboolean(L, ok);
            return 1;
        }

        static uint64_t FileTimeToTicks(const std::filesystem::file_time_type& ft) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(ft.time_since_epoch()).count();
        }
    } // anonymous namespace

    // ---------------------------- class methods --------------------------------

    ScriptingSystem::ScriptingSystem() : L(nullptr) {}
    ScriptingSystem::~ScriptingSystem() { if (L) { lua_close(L); L = nullptr; } }

    void ScriptingSystem::Initialize() {
        // 1) Register ScriptComponent (your engine’s pattern)
        ecsInterface.RegisterComponent<ScriptEntry>();
        ecsInterface.RegisterComponent<ScriptProperties>();

        // 2) Set system signature
        Signature sig;
        sig.set(ecsInterface.GetComponentType<ScriptProperties>(), true);
        ecsInterface.SetSystemSignature<ScriptingSystem>(sig);
        std::cout << "Signature for Scripting system is: " << sig << std::endl;

        // 3) Boot Lua
        L = luaL_newstate();
        luaL_openlibs(L);

        // 4) Expose `this` to Lua (so C functions can find the system)
        lua_pushlightuserdata(L, this);
        lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey);

        // 5) Register C functions (global names in Lua)
        lua_register(L, "IsKeyDown", l_IsKeyDown);
        lua_register(L, "CameraMove", l_CameraMove);
        lua_register(L, "NudgeLight", l_NudgeLight);
        lua_register(L, "ToggleLight", l_ToggleLight);
        lua_register(L, "SetHDR", l_SetHDR);
        lua_register(L, "SetBloom", l_SetBloom);
        lua_register(L, "SetGroundMode", l_SetGround);
        lua_register(L, "GetPosition", lua_GetPosition);
        lua_register(L, "SetPosition", lua_SetPosition);
        lua_register(L, "SpawnPrompt", l_spawnPrompt);
        lua_register(L, "DestroyEntity", l_DestroyEntity);
        lua_register(L, "IsMouseButtonDown", l_IsMouseButtonDown); 

        // Engine utility
        lua_register(L, "FindEntityByName", lua_FindEntityByName);
        lua_register(L, "SetMesh", lua_SetMesh);
        lua_register(L, "SetTexture", lua_SetTexture);

        // Navmesh
        lua_register(L, "PathfindTo", [](lua_State* L) -> int {
            const char* agentName = luaL_checkstring(L, 1);
            const char* targetName = luaL_checkstring(L, 2);

            ScriptPathfindTo(agentName, targetName);
            return 0;
            });

        // Create Cook table with our functions
        lua_newtable(L);                           // Cook
        lua_pushcfunction(L, l_Cook_WindowConfig); lua_setfield(L, -2, "WindowConfig");
        lua_pushcfunction(L, l_Cook_Audio);        lua_setfield(L, -2, "Audio");
        lua_pushcfunction(L, l_Cook_Textures);     lua_setfield(L, -2, "Textures");
        lua_pushcfunction(L, l_Cook_Meshes);       lua_setfield(L, -2, "Meshes");
        lua_pushcfunction(L, l_Cook_Prefab);       lua_setfield(L, -2, "Prefabs");
        lua_pushcfunction(L, l_Cook_Scene);        lua_setfield(L, -2, "Scenes");
        lua_pushcfunction(L, l_Cook_Material);     lua_setfield(L, -2, "Materials");
        lua_setglobal(L, "Cook");

        std::cout << "[ScriptingSystem] Initialized (Lua VM ready)\n";
    }

    void ScriptingSystem::Update(float dt) {
        for (auto const& e : mEntities) {
            if (!ecsInterface.HasComponent<ScriptProperties>(e)) continue;
            auto& comp = ecsInterface.GetComponent<ScriptProperties>(e);

            for (auto& sc : comp.scripts) {         // <--- NEW
                if (sc.scriptPath.empty()) continue;

                if (sc.hotReload && !sc.scriptPath.empty()) {
                    try {
                        auto nowTicks = FileTimeToTicks(
                            std::filesystem::last_write_time(sc.scriptPath));

                        if (nowTicks != sc.lastWriteTicks) {
                            std::cout << "[HotReload] " << sc.scriptPath << "\n";

                            if (LoadScriptTable(sc)) {
                                // re-expose entity id
                                lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                                lua_pushinteger(L, static_cast<lua_Integer>(e));
                                lua_setfield(L, -2, "entity");
                                lua_pop(L, 1);

                                // run on_init again so the script can re-bind state
                                CallHook(sc.tableRef, "on_init");
                                sc.initialized = true;
                            }

                            sc.lastWriteTicks = nowTicks; // remember the new time
                        }
                    }
                    catch (const std::exception& ex) {
                        std::cerr << "[HotReload] check failed for " << sc.scriptPath << ": " << ex.what() << "\n";
                    }
                }

                if (sc.tableRef == LUA_NOREF) {
                    if (!LoadScriptTable(sc)) continue;

                    // expose entity id: self.entity
                    lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                    lua_pushinteger(L, static_cast<lua_Integer>(e));
                    lua_setfield(L, -2, "entity");
                    lua_pop(L, 1);
                }
                if (!sc.initialized) { CallHook(sc.tableRef, "on_init"); sc.initialized = true; }
                CallHook(sc.tableRef, "on_update", dt);
            }
        }
    }

    void ScriptingSystem:: SetProjectRoot(const std::string& root) {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(root, ec);
        if (!ec) {
            s_ProjectRoot = canonical.generic_string();
            std::cout << "[ScriptingSystem] Project root: " << s_ProjectRoot << "\n";
        }
    }

     std::string ScriptingSystem::GetProjectRoot() { return s_ProjectRoot; }

     // For Event
     void ScriptingSystem::CallEntityFunction(Entity entity, const std::string& functionName, Entity arg1, Entity arg2)
     {
         // 1. Check if entity is valid and has the correct component
         if (!ecsInterface.IsEntityValid(entity) || !ecsInterface.HasComponent<ScriptProperties>(entity)) {
             return;
         }

         auto& comp = ecsInterface.GetComponent<ScriptProperties>(entity);

         // 2. Loop through all script instances on that entity
         for (auto& sc : comp.scripts) {
             if (sc.tableRef == LUA_NOREF) continue; // Script not loaded

             // 3. Get the Lua function
             // Push the instance table
             lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);  // [stack: tbl]

             // Push the method from the table
             lua_getfield(L, -1, functionName.c_str());       // [stack: tbl, fn]

             // Check if it's a valid function
             if (!lua_isfunction(L, -1)) {
                 lua_pop(L, 2); // pop non-function and tbl
                 continue;      // Try the next script on this entity
             }

             // 4. Push arguments
             // 'self' (the table) as first arg
             lua_pushvalue(L, -2); // [stack: tbl, fn, tbl]

             // Push our custom args
             lua_pushinteger(L, static_cast<lua_Integer>(arg1)); // [stack: tbl, fn, tbl, arg1]
             lua_pushinteger(L, static_cast<lua_Integer>(arg2)); // [stack: tbl, fn, tbl, arg1, arg2]

             int nargs = 3; // 3 arguments: self, arg1, arg2

             // 5. Call the function
             if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
                 std::cerr << "[Lua] call " << functionName << " error: "
                     << lua_tostring(L, -1) << "\n";
                 lua_pop(L, 1); // pop error
             }

             // 6. Clean up stack (pop the original table)
             lua_pop(L, 1); // pop tbl
         }
     }

     void ScriptingSystem::CallEntityFunction(Entity entity, const std::string& functionName, Entity arg1)
     {
         CallEntityFunction(entity, functionName, arg1, -1);
     }



    std::string ScriptingSystem::GetName() { return "ScriptingSystem"; }

    bool ScriptingSystem::LoadScriptTable(ScriptEntry& sc)
    {
        if (sc.tableRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, sc.tableRef);
            sc.tableRef = LUA_NOREF;
        }

        // BUILD ABSOLUTE PATH FROM PROJECT ROOT
        std::filesystem::path scriptPath;
        if (std::filesystem::path(sc.scriptPath).is_absolute()) {
            scriptPath = sc.scriptPath;
        }
        else {
            scriptPath = std::filesystem::path(s_ProjectRoot) / sc.scriptPath;
        }

        std::error_code ec;
        auto canonicalPath = std::filesystem::weakly_canonical(scriptPath, ec);

        if (ec || !std::filesystem::exists(canonicalPath)) {
            std::cerr << "[Lua] script file not found: " << sc.scriptPath
                << " (resolved to: " << scriptPath << ")\n";
            return false;
        }

        std::string absoluteScriptPath = canonicalPath.generic_string();

        // USE ABSOLUTE PATH
        if (luaL_dofile(L, absoluteScriptPath.c_str()) != LUA_OK) {
            std::cerr << "[Lua] load error in " << sc.scriptPath << ": " << lua_tostring(L, -1) << "\n";
            lua_pop(L, 1);
            return false;
        }

        if (!lua_istable(L, -1)) {
            std::cerr << "[Lua] script must `return` a table: " << sc.scriptPath << "\n";
            lua_pop(L, 1);
            return false;
        }

        sc.tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
        sc.initialized = false;
        return true;
    }

    bool ScriptingSystem::CallHook(int tableRef, const char* fn, float arg)
    {
        if (tableRef <= 0 || fn == nullptr)
            return false;

        // Push the instance table
        lua_rawgeti(L, LUA_REGISTRYINDEX, tableRef);  // [tbl]
        lua_getfield(L, -1, fn);                      // [tbl, fn]

        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);                            // pop fn, tbl
            return false;
        }

        // self (the table) as first arg
        lua_pushvalue(L, -2);                         // [tbl, fn, tbl]
        int nargs = 1;

        if (std::strcmp(fn, "on_update") == 0) {
            lua_pushnumber(L, arg);                  // [tbl, fn, tbl, dt]
            nargs = 2;
        }

        if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::cerr << "[Lua Error] in " << fn << ": " << (err ? err : "(unknown)") << "\n";
            lua_pop(L, 1); // pop error
            lua_pop(L, 1); // pop tbl
            return false;
        }

        lua_pop(L, 1); // pop tbl
        return true;
    }
    
    Entity ScriptingSystem::CreateScriptEntity(const std::string& path) {
        Entity e = ecsInterface.CreateEntity();

        ScriptProperties comp;
        ScriptEntry inst;
        inst.scriptPath = path;
        comp.scripts.push_back(inst);

        ecsInterface.AddComponent<ScriptProperties>(e, comp);
        ecsInterface.AddComponent<TransformComponent>(e, TransformComponent{});
        ecsInterface.AddComponent<HierarchyComponent>(e, HierarchyComponent{});
        return e;
    }

    void ScriptingSystem::HotReloadScript(const std::string& changedPath)
    {
        namespace fs = std::filesystem;
        std::error_code ec;

        fs::path changedAbs = fs::weakly_canonical(changedPath, ec);
        std::string changedNorm = (ec ? fs::path(changedPath) : changedAbs)
            .lexically_normal().generic_string();
        std::string changedFile = fs::path(changedNorm).filename().string();

        std::cout << "[HotReload] request for: " << changedNorm << "\n";

        bool any = false;

        for (auto e : mEntities)
        {
            if (!ecsInterface.HasComponent<ScriptProperties>(e)) continue;
            auto& comp = ecsInterface.GetComponent<ScriptProperties>(e);

            for (auto& sc : comp.scripts) {
                if (sc.scriptPath.empty()) continue;

                ec.clear();
                fs::path scAbs = fs::weakly_canonical(sc.scriptPath, ec);
                std::string scNorm = (ec ? fs::path(sc.scriptPath) : scAbs)
                    .lexically_normal().generic_string();
                std::string scFile = fs::path(scNorm).filename().string();

                bool match = (scNorm == changedNorm) || (scFile == changedFile);

                if (match)
                {
                    std::cout << "  -> reloading entity " << e
                        << " (" << scNorm << ")\n";
                    if (LoadScriptTable(sc))
                    {
                        lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                        lua_pushinteger(L, static_cast<lua_Integer>(e));
                        lua_setfield(L, -2, "entity");
                        lua_pop(L, 1);

                        sc.initialized = false;
                        CallHook(sc.tableRef, "on_init");
                        any = true;

                        std::error_code ecc;
                        auto ft = std::filesystem::last_write_time(sc.scriptPath, ecc);
                        if (!ecc)
                        {
                            sc.lastWriteTicks = FileTimeToTicks(ft);
                        }
                    }
                    else
                    {
                        std::cerr << "  !! reload failed for " << scNorm << "\n";
                    }
                }
            }
        }
    }

} // namespace Framework
