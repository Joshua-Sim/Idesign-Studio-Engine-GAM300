#include "pch.h"
#include "ScriptingSystem.h"
#include "Coordinator.h"
#include "ComponentList.h"
#include "Graphics.h"          // to call Graphics* methods
#include <lua.hpp>
#include <iostream>
#include "AssetCompiler.h"
#include "WindowConfigProperties.h"  // g_WindowConfigProps
#include "AudioLoader.h"             // g_AudioPropertiesProps
#include "TextureLoader.h"           // g_TexturePropertiesProps
#include "MeshProperties.h"          // g_MeshPropertiesProps
#include <filesystem>

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

        namespace fs = std::filesystem;

        // Join a relative path with fallback prefixes until a file is found.
        static std::string resolve_path(const std::string& rel) {
            const char* roots[] = {
                ".",                // current working dir
                "..",               // one up
                "../..",            // two up (depending on VS layout)
            };

            for (auto r : roots) {
                fs::path p = fs::path(r) / rel;
                std::error_code ec;
                if (fs::exists(p, ec)) return fs::weakly_canonical(p, ec).string();
            }
            // if not found, return as-is (fopen will fail; we’ll log)
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

        // Cook.Audio(jsonPath, outPath [, key="AudioProperties"])
        int l_Cook_Audio(lua_State* L) {
            const char* json = luaL_checkstring(L, 1);
            const char* out = luaL_checkstring(L, 2);
            const char* desc = luaL_checkstring(L, 3);
            const char* key = luaL_optstring(L, 4, "AudioProperties");

            bool skipped = prepare_output_and_skip_if_fresh(json, out);
            bool ok = skipped ? true : CompileArray<AudioProperties>(json, out, desc, g_AudioPropertiesProps, key);
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
    
        static uint64_t FileTimeToTicks(const std::filesystem::file_time_type& ft) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(ft.time_since_epoch()).count();
        }
    } // anonymous namespace

    // ---------------------------- class methods --------------------------------

    ScriptingSystem::ScriptingSystem() : L(nullptr) {}
    ScriptingSystem::~ScriptingSystem() { if (L) { lua_close(L); L = nullptr; } }

    void ScriptingSystem::Initialize() {
        // 1) Register ScriptComponent (your engine’s pattern)
        ecsInterface.RegisterComponent<ScriptComponent>();

        // 2) Set system signature
        Signature sig;
        sig.set(ecsInterface.GetComponentType<ScriptComponent>(), true);
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

        // Create Cook table with our functions
        lua_newtable(L);                           // Cook
        lua_pushcfunction(L, l_Cook_WindowConfig); lua_setfield(L, -2, "WindowConfig");
        lua_pushcfunction(L, l_Cook_Audio);        lua_setfield(L, -2, "Audio");
        lua_pushcfunction(L, l_Cook_Textures);     lua_setfield(L, -2, "Textures");
        lua_pushcfunction(L, l_Cook_Meshes);       lua_setfield(L, -2, "Meshes");
        lua_setglobal(L, "Cook");

        std::cout << "[ScriptingSystem] Initialized (Lua VM ready)\n";
    }

    void ScriptingSystem::Update(float dt) {
        for (auto const& e : mEntities) {
            if (!ecsInterface.HasComponent<ScriptComponent>(e)) continue;
            auto& sc = ecsInterface.GetComponent<ScriptComponent>(e);

            if (sc.hotReload && !sc.scriptPath.empty()) {
                try {
                    auto nowTicks = FileTimeToTicks(std::filesystem::last_write_time(sc.scriptPath));
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

    std::string ScriptingSystem::GetName() { return "ScriptingSystem"; }

    bool ScriptingSystem::LoadScriptTable(ScriptComponent& sc)
    {
        // Clean any previous instance table
        if (sc.tableRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, sc.tableRef);
            sc.tableRef = LUA_NOREF;
        }

        // Execute the Lua file (must return a table)
        if (luaL_dofile(L, sc.scriptPath.c_str()) != LUA_OK) {
            std::cerr << "[Lua] load error in " << sc.scriptPath
                << ": " << lua_tostring(L, -1) << "\n";
            lua_pop(L, 1); // pop error
            return false;
        }

        if (!lua_istable(L, -1)) {
            std::cerr << "[Lua] script must `return` a table: " << sc.scriptPath << "\n";
            lua_pop(L, 1); // pop non-table
            return false;
        }

        // Store table in registry and reset lifecycle
        sc.tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
        sc.initialized = false;
        return true;
    }

    bool ScriptingSystem::CallHook(int tableRef, const char* fn, float arg)
    {
        if (tableRef == LUA_NOREF || fn == nullptr) return false;

        // Push the instance table
        lua_rawgeti(L, LUA_REGISTRYINDEX, tableRef);      // [tbl]
        // Push the method from the table
        lua_getfield(L, -1, fn);                          // [tbl, fn]

        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);                                // pop fn, tbl
            return false;
        }

        // self (the table) as first arg
        lua_pushvalue(L, -2);                             // [tbl, fn, tbl]
        int nargs = 1;

        if (std::strcmp(fn, "on_update") == 0) {
            lua_pushnumber(L, arg);                       // [tbl, fn, tbl, dt]
            nargs = 2;
        }

        if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
            std::cerr << "[Lua] call " << fn << " error: "
                << lua_tostring(L, -1) << "\n";
            lua_pop(L, 1);                                // pop error
            lua_pop(L, 1);                                // pop tbl
            return false;
        }

        lua_pop(L, 1);                                    // pop tbl
        return true;
    }
    
    Entity ScriptingSystem::CreateScriptEntity(const std::string& path) {
        Entity e = ecsInterface.CreateEntity();
        ecsInterface.AddComponent<ScriptComponent>(e, ScriptComponent{ path });
        ecsInterface.AddComponent<TransformComponent>(e,TransformComponent{});
        ecsInterface.AddComponent<HierarchyComponent>(e, HierarchyComponent{});
        return e;
    }

    void ScriptingSystem::HotReloadScript(const std::string& changedPath)
    {
        namespace fs = std::filesystem;
        std::error_code ec;

        // Normalize the changed path
        fs::path changedAbs = fs::weakly_canonical(changedPath, ec);
        std::string changedNorm = (ec ? fs::path(changedPath) : changedAbs).lexically_normal().generic_string();
        std::string changedFile = fs::path(changedNorm).filename().string();

        std::cout << "[HotReload] request for: " << changedNorm << "\n";

        bool any = false;

        for (auto e : mEntities)
        {
            if (!ecsInterface.HasComponent<ScriptComponent>(e)) continue;
            auto& sc = ecsInterface.GetComponent<ScriptComponent>(e);
            if (sc.scriptPath.empty()) continue;

            // Normalize the stored script path
            ec.clear();
            fs::path scAbs = fs::weakly_canonical(sc.scriptPath, ec);
            std::string scNorm = (ec ? fs::path(sc.scriptPath) : scAbs).lexically_normal().generic_string();
            std::string scFile = fs::path(scNorm).filename().string();

            // Match by absolute OR by filename (helps when one side is relative)
            bool match = (scNorm == changedNorm) || (scFile == changedFile);

            if (match)
            {
                std::cout << "  -> reloading entity " << e << " (" << scNorm << ")\n";
                if (LoadScriptTable(sc))
                {
                    // re-expose entity id on the table and re-init
                    lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                    lua_pushinteger(L, static_cast<lua_Integer>(e));
                    lua_setfield(L, -2, "entity");
                    lua_pop(L, 1);

                    sc.initialized = false;
                    CallHook(sc.tableRef, "on_init");
                    any = true;
                }
                else
                {
                    std::cerr << "  !! reload failed for " << scNorm << "\n";
                }
            }
        }

        // Optional fallback: if nothing matched but it was a .lua file, reload ALL scripts
        if (!any && fs::path(changedNorm).extension() == ".lua")
        {
            std::cout << "  (no direct match; reloading all scripts)\n";
            for (auto e : mEntities)
            {
                if (!ecsInterface.HasComponent<ScriptComponent>(e)) continue;
                auto& sc = ecsInterface.GetComponent<ScriptComponent>(e);
                if (sc.scriptPath.empty()) continue;

                if (LoadScriptTable(sc))
                {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                    lua_pushinteger(L, static_cast<lua_Integer>(e));
                    lua_setfield(L, -2, "entity");
                    lua_pop(L, 1);

                    sc.initialized = false;
                    CallHook(sc.tableRef, "on_init");
                }
            }
        }
    }

} // namespace Framework
