#include "pch.h"
#include "ScriptingSystem.h"
#include "Coordinator.h"
#include "Graphics.h"          // to call Graphics* methods
#include <lua.hpp>
#include <iostream>
#include "AssetCompiler.h"
#include "WindowConfigProperties.h"  // g_WindowConfigProps
#include "TextureLoader.h"           // g_TexturePropertiesProps
#include "NavMeshSystem.h"
#include "MeshProperties.h"          // g_MeshPropertiesProps
#include <filesystem>
#include "ScriptProperties.h"
#include "AudioSystem.h"
#include "ItemComponent.h" // <--- Add this
#include <GameSystem.h>
#include <PhysicsSystem.h>
#include "EngineState.h"
#include "ButtonComponent.h"
#include "ButtonSystem.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "Inventory.h" // Include your Inventory header
#include "BoxColliderComponent.h"
#include "InputHandler.h"
#include "SceneInterface.h"

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

        static int lua_GetMousePosition(lua_State* L)
        {
            // Access your singleton InputHandler
            auto* input = Framework::InputHandler::GetInstance();

            if (input)
            {
                // Push X and Y to Lua
                lua_pushnumber(L, input->GetMouseX());
                lua_pushnumber(L, input->GetMouseY());
                return 2; // We are returning 2 values (x, y)
            }

            // Fallback if system is missing
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            return 2;
        }

        //For text component
        static int lua_GetTextComponent(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            if (!ecsInterface.IsEntityValid(e) || !ecsInterface.HasComponent<TextProperties>(e)) {
                lua_pushnil(L);
                return 1;
            }

            auto& text = ecsInterface.GetComponent<TextProperties>(e);

            // Return a table with text component data
            lua_newtable(L);

            lua_pushstring(L, text.text.c_str());
            lua_setfield(L, -2, "text");

            lua_pushnumber(L, text.scale);
            lua_setfield(L, -2, "scale");

            // Color
            lua_newtable(L);
            lua_pushnumber(L, text.color.r); lua_setfield(L, -2, "r");
            lua_pushnumber(L, text.color.g); lua_setfield(L, -2, "g");
            lua_pushnumber(L, text.color.b); lua_setfield(L, -2, "b");
            lua_setfield(L, -2, "color");

            lua_pushinteger(L, text.alignment);
            lua_setfield(L, -2, "alignment");

            lua_pushboolean(L, text.isBillboard);
            lua_setfield(L, -2, "isBillboard");

            lua_pushboolean(L, text.screenSpace);
            lua_setfield(L, -2, "screenSpace");

            return 1;
        }

        static int lua_SetTextComponent(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* field = luaL_checkstring(L, 2);
            // Value is at index 3 (type depends on field)

            if (!ecsInterface.IsEntityValid(e) || !ecsInterface.HasComponent<TextProperties>(e)) {
                return luaL_error(L, "SetTextComponent: invalid entity or missing TextProperties");
            }

            auto& text = ecsInterface.GetComponent<TextProperties>(e);

            if (strcmp(field, "text") == 0) {
                text.text = luaL_checkstring(L, 3);
            }
            else if (strcmp(field, "scale") == 0) {
                text.scale = static_cast<float>(luaL_checknumber(L, 3));
            }
            else if (strcmp(field, "alignment") == 0) {
                text.alignment = static_cast<int>(luaL_checkinteger(L, 3));
            }
            else if (strcmp(field, "isBillboard") == 0) {
                text.isBillboard = lua_toboolean(L, 3);
            }
            else if (strcmp(field, "screenSpace") == 0) {
                text.screenSpace = lua_toboolean(L, 3);
            }
            else if (strcmp(field, "color") == 0) {
                // Expect a table with r, g, b
                if (!lua_istable(L, 3)) {
                    return luaL_error(L, "SetTextComponent: color must be a table {r, g, b}");
                }

                lua_getfield(L, 3, "r");
                lua_getfield(L, 3, "g");
                lua_getfield(L, 3, "b");

                text.color.r = static_cast<float>(luaL_checknumber(L, -3));
                text.color.g = static_cast<float>(luaL_checknumber(L, -2));
                text.color.b = static_cast<float>(luaL_checknumber(L, -1));

                lua_pop(L, 3);
            }
            else {
                return luaL_error(L, "SetTextComponent: unknown field '%s'", field);
            }

            return 0;
        }

        static int l_spawnCauldronItem(lua_State* L) {
            // 1. Get Arguments
            Entity parentID = static_cast<Entity>(luaL_checkinteger(L, 1));
            int itemID = (int)luaL_checkinteger(L, 2);

            // 2. Validate Parent
            if (!ecsInterface.IsEntityValid(parentID) ||
                !ecsInterface.HasComponent<TransformComponent>(parentID)) {
                return 0;
            }

            // 3. Validate Item ID
            if (itemID < 0 || itemID >= Framework::kItemCount) {
                std::cout << "[Lua Error] Invalid Item ID: " << itemID << "\n";
                return 0;
            }

            std::string itemName = Framework::kItemNames[itemID];

            // =========================================================
            // PATH HANDLING LOGIC (Robust)
            // =========================================================

            // 1. Define Relative Paths
            std::string relObjPath = "Assets/Objects/" + itemName + ".obj";
            std::string relPngPath = "Assets/Textures/" + itemName + "3D.png";

            // 2. Resolve Mesh Path
            fs::path meshPath(relObjPath);
            if (!meshPath.is_absolute()) {
                // If relative, prepend Current Working Directory (Project Root)
                meshPath = fs::current_path() / meshPath;
            }

            // 3. Resolve Texture Path
            fs::path texPath(relPngPath);
            if (!texPath.is_absolute()) {
                texPath = fs::current_path() / texPath;
            }

            // 4. Debug: Verify existence (Helps you see if path is wrong)
            if (!fs::exists(meshPath)) {
                std::cout << "[Lua Error] MESH MISSING at: " << meshPath.string() << std::endl;
                // Optional: Fallback to a default cube if missing
                // meshPath = fs::current_path() / "Assets/Objects/cube.obj";
            }
            if (!fs::exists(texPath)) {
                std::cout << "[Lua Error] TEXTURE MISSING at: " << texPath.string() << std::endl;
            }

            // =========================================================

            // 5. Calculate Position (Above BoxCollider)
            auto& parentTrans = ecsInterface.GetComponent<TransformComponent>(parentID);
            glm::vec3 spawnPos = parentTrans.position;

            // 6. Create Entity
            Entity itemEntity = ecsInterface.CreateEntity();

            // --- NEW: SET UNIQUE NAME ---
            std::string uniqueName = itemName + "_" + std::to_string((unsigned int)itemEntity);

            // Assuming you have a SetEntityName function in your ECS Interface
            ecsInterface.SetEntityName(itemEntity, uniqueName);
            std::cout << "[DEBUG C++] Created Entity: " << uniqueName << std::endl;
            // ----------------------------

            // 7. Transform
            TransformComponent tf;
            tf.position = spawnPos;
            tf.scale = glm::vec3(0.3f);
            tf.rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
            ecsInterface.AddComponent(itemEntity, tf);

            // 8. Hierarchy
            HierarchyComponent hc;
            ecsInterface.AddComponent(itemEntity, hc);

            // 9. Mesh (Use the resolved absolute path)
            MeshProperties mp;
            mp.path = meshPath.string(); // Convert fs::path back to string
            MeshLoader::loadMesh(mp.path, mp);
            ecsInterface.AddComponent(itemEntity, mp);

            // 10. Texture (Use the resolved absolute path)
            TextureProperties tp;
            tp.path = texPath.string(); // Convert fs::path back to string
            TextureLoader::loadTexture(tp, true);
            ecsInterface.AddComponent(itemEntity, tp);

            ItemComponent itemComp;
            itemComp.name = itemName;
            itemComp.isIngredient = true;
            itemComp.isChoppable = false;
            itemComp.toDestroyWhenPicked = true; // Crucial: C++ destroys it on pickup
            ecsInterface.AddComponent(itemEntity, itemComp);

            TagComponent tc;
            tc.tags = { "Interactable" };
            ecsInterface.AddComponent(itemEntity, tc);

            PhysicsProperties pp;
            pp.isImmovable = true;
            pp.category = 1;
            ecsInterface.AddComponent(itemEntity, pp);

            BoxColliderComponent colliderComp;
            BoxColliderEntry entry;
            entry.size = glm::vec3(1.0f); 
            entry.isInteractable = true; 
            entry.promptText = "Pick up: " + itemName;
            colliderComp.colliders.push_back(entry);
            ecsInterface.AddComponent(itemEntity, colliderComp);

            lua_pushinteger(L, static_cast<lua_Integer>(itemEntity));
            return 1;
        }

        static int l_SetInteractionText(lua_State* L)
        {
            // Arg 1: Entity ID
            Entity entity = static_cast<Entity>(luaL_checkinteger(L, 1));

            // Arg 2: New Text
            const char* newText = luaL_checkstring(L, 2);

            // Logic: Find the collider and update the string
            if (ecsInterface.IsEntityValid(entity))
            {
                if (ecsInterface.HasComponent<BoxColliderComponent>(entity))
                {
                    auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(entity);

                    // Loop through all colliders attached to this entity
                    for (auto& entry : colliderComp.colliders)
                    {
                        // We only want to update the ones marked as Interactable
                        if (entry.isInteractable)
                        {
                            entry.promptText = std::string(newText);
                        }
                    }
                }
            }

            return 0; // Returns nothing to Lua
        }

        static int l_DestroyEntity(lua_State* L) {
            // 1. Get Entity ID from Lua
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            // 2. Check validity
            if (ecsInterface.IsEntityValid(e)) {
             
                sceneManager.m_PendingEntityDeletes.push_back(e);
                // std::cout << "[Lua] Queued Entity " << e << " for destruction" << std::endl;
            }
            else {
                std::cout << "[Lua Warning] Tried to destroy invalid entity: " << e << std::endl;
            }
            return 0;
        }

        //Event firing
        static int lua_FireEvent(lua_State* L)
        {
            // Arg 1: Event Message Name (string)
            const char* messageName = luaL_checkstring(L, 1);

            // Arg 2: Sender Entity (optional, defaults to 0)
            Entity senderID = 0;
            if (lua_gettop(L) >= 2) {
                senderID = static_cast<Entity>(luaL_checkinteger(L, 2));
            }

            // Arg 3: Target Entity (optional, defaults to 0)
            Entity targetID = 0;
            if (lua_gettop(L) >= 3) {
                targetID = static_cast<Entity>(luaL_checkinteger(L, 3));
            }

            // Arg 4: Action Type (optional, defaults to 0)
            int actionType = 0;
            if (lua_gettop(L) >= 4) {
                actionType = static_cast<int>(luaL_checkinteger(L, 4));
            }

            // Build Payload
            CollisionEventData payload;
            payload.senderID = senderID;
            payload.targetID = targetID;
            payload.actionType = actionType;

            // Fire Event
            EventManager::Instance().Fire(messageName, &payload);

            std::cout << "[Lua] Fired Event: " << messageName << std::endl;

            return 0;
        }

        // Set a script's variable (Access a scripts local var and edit if needed)
        static int lua_SetScriptVariable(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* varName = luaL_checkstring(L, 2);
            const char* value = luaL_checkstring(L, 3);

            if (!ecsInterface.IsEntityValid(e) || !ecsInterface.HasComponent<ScriptProperties>(e)) {
                return luaL_error(L, "SetScriptVariable: invalid entity or no ScriptProperties");
            }

            auto& scriptProps = ecsInterface.GetComponent<ScriptProperties>(e);

            // Find the Customer script's table
            for (auto& sc : scriptProps.scripts) {
                if (sc.tableRef != LUA_NOREF && sc.tableRef > 0) {
                    // Push the script table
                    lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);

                    // Set the variable
                    lua_pushstring(L, value);
                    lua_setfield(L, -2, varName);

                    // Pop the table
                    lua_pop(L, 1);
                }
            }

            return 0;
        }

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

       // Navmesh build function
        static int lua_BuildNavMesh(lua_State* L)
        {
            auto& ecs = ecsInterface;
            auto navSystem = ecs.GetSystem<Framework::NavMeshSystem>();

            if (!navSystem)
            {
                std::cerr << "[Lua] NavMeshSystem not found!\n";
                lua_pushboolean(L, false);
                return 1;
            }

            // If your function is void and doesn't return success:
            navSystem->BuildNavMesh();  // or whatever your method is called
            std::cout << "[Lua] NavMesh build triggered.\n";

            lua_pushboolean(L, true);
            return 1;
        }

        //Spawn prefab expose
        static int lua_SpawnPrefab(lua_State* L)
        {
            const char* prefabIdentifier = luaL_checkstring(L, 1);

            Entity spawnedEntity = std::numeric_limits<Entity>::max();

            int numArgs = lua_gettop(L);

            if (numArgs == 1)
            {
                // Try as GUID first (direct lookup - faster)
                spawnedEntity = InstantiatePrefab(std::string(prefabIdentifier));

                // If that fails, try as name
                if (spawnedEntity == std::numeric_limits<Entity>::max())
                {
                    spawnedEntity = InstantiatePrefabByName(std::string(prefabIdentifier));
                }
            }
            else if (numArgs == 2)
            {
                // Spawn at another entity's position
                Entity positionEntity = static_cast<Entity>(luaL_checkinteger(L, 2));

                if (!ecsInterface.IsEntityValid(positionEntity) ||
                    !ecsInterface.HasComponent<TransformComponent>(positionEntity))
                {
                    return luaL_error(L, "SpawnPrefab: invalid position entity or missing TransformComponent");
                }

                auto& posTransform = ecsInterface.GetComponent<TransformComponent>(positionEntity);

                // Try GUID first
                spawnedEntity = InstantiatePrefab(std::string(prefabIdentifier), posTransform.position);

                // Try name if GUID failed
                if (spawnedEntity == std::numeric_limits<Entity>::max())
                {
                    spawnedEntity = InstantiatePrefabByName(std::string(prefabIdentifier), posTransform.position);
                }
            }
            else if (numArgs == 4)
            {
                // Spawn with explicit position (x, y, z)
                float x = static_cast<float>(luaL_checknumber(L, 2));
                float y = static_cast<float>(luaL_checknumber(L, 3));
                float z = static_cast<float>(luaL_checknumber(L, 4));

                glm::vec3 position(x, y, z);

                // Try GUID first
                spawnedEntity = InstantiatePrefab(std::string(prefabIdentifier), position);

                // Try name if GUID failed
                if (spawnedEntity == std::numeric_limits<Entity>::max())
                {
                    spawnedEntity = InstantiatePrefabByName(std::string(prefabIdentifier), position);
                }
            }
            else if (numArgs == 10)
            {
                // Full transform
                float px = static_cast<float>(luaL_checknumber(L, 2));
                float py = static_cast<float>(luaL_checknumber(L, 3));
                float pz = static_cast<float>(luaL_checknumber(L, 4));

                float rx = static_cast<float>(luaL_checknumber(L, 5));
                float ry = static_cast<float>(luaL_checknumber(L, 6));
                float rz = static_cast<float>(luaL_checknumber(L, 7));

                float sx = static_cast<float>(luaL_checknumber(L, 8));
                float sy = static_cast<float>(luaL_checknumber(L, 9));
                float sz = static_cast<float>(luaL_checknumber(L, 10));

                // Try GUID first
                spawnedEntity = InstantiatePrefab(
                    std::string(prefabIdentifier),
                    glm::vec3(px, py, pz),
                    glm::vec3(rx, ry, rz),
                    glm::vec3(sx, sy, sz)
                );

                // Try name if GUID failed
                if (spawnedEntity == std::numeric_limits<Entity>::max())
                {
                    spawnedEntity = InstantiatePrefabByName(
                        std::string(prefabIdentifier),
                        glm::vec3(px, py, pz),
                        glm::vec3(rx, ry, rz),
                        glm::vec3(sx, sy, sz)
                    );
                }
            }
            else
            {
                return luaL_error(L, "SpawnPrefab: invalid number of arguments (expected 1, 2, 4, or 10)");
            }

            // Return the spawned entity ID (or nil if failed)
            if (spawnedEntity == std::numeric_limits<Entity>::max())
            {
                lua_pushnil(L);
                return 1;
            }

            lua_pushinteger(L, static_cast<lua_Integer>(spawnedEntity));
            return 1;
        }

        // --------- Tag management functions--------------

        static int lua_GetEntitiesByTag(lua_State* L)
        {
            const char* tag = luaL_checkstring(L, 1);

            std::vector<Entity> entities = ecsInterface.GetEntitiesByTag(tag);

            // Create a Lua table to return the entities
            lua_newtable(L);

            for (size_t i = 0; i < entities.size(); ++i)
            {
                lua_pushinteger(L, static_cast<lua_Integer>(entities[i]));
                lua_rawseti(L, -2, static_cast<int>(i + 1)); // Lua arrays start at 1
            }

            return 1; // Return the table
        }

        static int lua_AddTagToEntity(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* tag = luaL_checkstring(L, 2);

            if (!ecsInterface.IsEntityValid(e))
            {
                return luaL_error(L, "AddTagToEntity: invalid entity %d", (int)e);
            }

            ecsInterface.AddTagToEntity(e, tag);
            return 0;
        }

        static int lua_RemoveTagFromEntity(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* tag = luaL_checkstring(L, 2);

            if (!ecsInterface.IsEntityValid(e))
            {
                return luaL_error(L, "RemoveTagFromEntity: invalid entity %d", (int)e);
            }

            if (!ecsInterface.HasComponent<TagComponent>(e))
            {
                return 0; // Nothing to remove
            }

            auto& tagComp = ecsInterface.GetComponent<TagComponent>(e);
            auto it = std::find(tagComp.tags.begin(), tagComp.tags.end(), std::string(tag));

            if (it != tagComp.tags.end())
            {
                tagComp.tags.erase(it);
            }

            return 0;
        }

        static int lua_EntityHasTag(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* tag = luaL_checkstring(L, 2);

            bool hasTag = ecsInterface.EntityHasTag(e, tag);
            lua_pushboolean(L, hasTag);
            return 1;
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

        static int lua_SetRotation(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            float x = static_cast<float>(luaL_checknumber(L, 2));
            float y = static_cast<float>(luaL_checknumber(L, 3));
            float z = static_cast<float>(luaL_checknumber(L, 4));

            if (!ecsInterface.IsEntityValid(e) ||
                !ecsInterface.HasComponent<TransformComponent>(e))
            {
                return luaL_error(L, "SetRotation: entity %d has no TransformComponent", (int)e);
            }

            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);
            tr.rotation = glm::vec3(x, y, z);
            return 0;
        }

        static int lua_GetScore(lua_State* L)
        {
            int s = Framework::GameSystem::GetScore();
            lua_pushinteger(L, s);
            return 1; // 1 return value
        }

        static int lua_AddScore(lua_State* L)
        {
            int delta = (int)luaL_checkinteger(L, 1);
            Framework::GameSystem::AddScore(delta);
            return 0;
        }

        static int lua_SetText(lua_State* L)
        {
            int id = (int)luaL_checkinteger(L, 1);
            const char* newText = luaL_checkstring(L, 2);

            Framework::Entity e = static_cast<Framework::Entity>(id);

            if (ecsInterface.IsEntityValid(e) &&
                ecsInterface.HasComponent<Framework::TextProperties>(e))
            {
                auto& txt = ecsInterface.GetComponent<Framework::TextProperties>(e);
                txt.text = newText;
            }

            return 0;
        }

        // ============ AUDIO LUA BINDINGS ============
        static int lua_AudioPlay(lua_State* L) {
            const char* eventPath = luaL_checkstring(L, 1);
            if (g_AudioSystem) {
                g_AudioSystem->PlayAudio(eventPath);
            }
            return 0;
        }

        static int lua_AudioStop(lua_State* L) {
            if (g_AudioSystem) {
                g_AudioSystem->Reset();
            }
            return 0;
        }

        static int lua_AudioPause(lua_State* L) {
            if (g_AudioSystem) {
                g_AudioSystem->PauseAllAudio();
            }
            return 0;
        }

        static int lua_AudioResume(lua_State* L) {
            if (g_AudioSystem) {
                g_AudioSystem->ResumeAllAudio();
            }
            return 0;
        }

        static int lua_AudioSetVolume(lua_State* L) {
            float volume = (float)luaL_checknumber(L, 1);
            if (g_AudioSystem) {
                g_AudioSystem->SetMasterVolume(volume);
            }
            return 0;
        }

        static int lua_AudioPlay3D(lua_State* L) {
            const char* eventPath = luaL_checkstring(L, 1);
            float x = (float)luaL_checknumber(L, 2);
            float y = (float)luaL_checknumber(L, 3);
            float z = (float)luaL_checknumber(L, 4);

            if (g_AudioSystem) {
                g_AudioSystem->PlaySound3D(eventPath, Framework::Vector3(x, y, z));
            }
            return 0;
        }

        // ============ END AUDIO LUA BINDINGS ============


        // ============ GAME STATE LUA BINDINGS ============
        static int lua_GameStateStart(lua_State* L)
        {
            //Send message to start audio and other play-mode systems
            MessageBus::Get().SendImmediate(EnterPlayModeMessage());

            SceneInterface::Instance().SetMenu(true);

            return 0;
        }

        static int lua_GameStateResume(lua_State* L)
        {            
            // Set engine state to play mode
            engineState.SetPlay(true);
                    
            //Send message to start audio and other play-mode systems
            MessageBus::Get().SendImmediate(ResumePlayModeMessage());

            return 0;
        }

        static int lua_GameStatePaused(lua_State* L)
        {
            // Stop the game
            engineState.SetPlay(false);

            // Send exit message
            MessageBus::Get().SendImmediate(PausePlayModeMessage());

            return 0;
        }

        static int lua_GameStateExit(lua_State* L)
        {
            // Stop the game
            engineState.SetPlay(false);

            // Send exit message
            MessageBus::Get().SendImmediate(ExitPlayModeMessage());

            return 0;
        }

        // ============ END GAME STATE LUA BINDINGS ============


        static int Lua_GetItemName(lua_State* L) {
            int entityID = (int)lua_tointeger(L, 1);

            if (ecsInterface.IsEntityValid(entityID) &&
                ecsInterface.HasComponent<Framework::ItemComponent>(entityID))
            {
                auto& item = ecsInterface.GetComponent<Framework::ItemComponent>(entityID);
                lua_pushstring(L, item.name.c_str());
                return 1;
            }
            lua_pushstring(L, "");
            return 1;
        }

        // Get item name from item ID (item mapping to name)
        static int lua_GetItemNameById(lua_State* L) {
            int itemId = static_cast<int>(luaL_checkinteger(L, 1));

            if (itemId < 0 || itemId >= Framework::kItemCount) {
                lua_pushstring(L, "");
                return 1;
            }

            lua_pushstring(L, Framework::kItemNames[itemId]);
            return 1;
        }

        int Lua_AddToInventory(lua_State* L) {
            const char* itemName = lua_tostring(L, 1); // Arg 1: "RedMushroom"

            int itemId = Framework::ItemNameToId(itemName);
            if (itemId < 0) {
                lua_pushinteger(L, -1); // Error
                return 1;
            }

            auto& inv = Framework::Inventory::Instance();
            int slot = inv.AddFirstEmpty(itemId);

            lua_pushinteger(L, slot); // Return the slot index (or -1 if full)
            return 1;
        }

        int Lua_DestroyEntity(lua_State* L) {
            int entityID = (int)lua_tointeger(L, 1);

            // Ensure we clean up physics body first if needed
            auto physics = ecsInterface.GetSystem<Framework::PhysicsSystem>();
            if (physics) physics->RemoveBodyForEntity(entityID);

            ecsInterface.DestroyEntity(entityID);
            return 0;
        }

        // 1. Define the function
        static int lua_IsEntityValid(lua_State* L) {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            bool valid = ecsInterface.IsEntityValid(e);
            lua_pushboolean(L, valid);
            return 1;
        }

        static int Lua_IsItemDestroyOnPick(lua_State* L) {
            int entityID = (int)lua_tointeger(L, 1);

            if (ecsInterface.IsEntityValid(entityID) &&
                ecsInterface.HasComponent<Framework::ItemComponent>(entityID))
            {
                auto& item = ecsInterface.GetComponent<Framework::ItemComponent>(entityID);
                lua_pushboolean(L, item.toDestroyWhenPicked); // Return the bool
                return 1;
            }
            lua_pushboolean(L, false);
            return 1;
        }

        // --------- Scene Loading --------------
        static int Lua_RegisterButtonCallback(lua_State* L)
        {
            int entity = (int)luaL_checkinteger(L, 1);

            if (!lua_isfunction(L, 2))
                return luaL_error(L, "Second argument must be a function");
        // 1. Function to get the ID of the item currently held
        

            // Create reference to function
            lua_pushvalue(L, 2);
            int funcRef = luaL_ref(L, LUA_REGISTRYINDEX);

            if (!ecsInterface.HasComponent<ButtonComponent>(entity))
                return luaL_error(L, "Entity %d has no ButtonComponent", entity);

            auto& btn = ecsInterface.GetComponent<ButtonComponent>(entity);

            // Capture lua_State* and entity by value, because onClick is void()
            lua_State* L_state = L;

            btn.onClick = [L_state, funcRef, entity]()
                {
                    // Push the stored Lua function
                    lua_rawgeti(L_state, LUA_REGISTRYINDEX, funcRef);

                    // Pass the entity id as a Lua argument
                    lua_pushinteger(L_state, (lua_Integer)entity);

                    if (lua_pcall(L_state, 1, 0, 0) != LUA_OK)
                    {
                        std::cout << "[Lua] Button callback error: "
                            << lua_tostring(L_state, -1) << "\n";
                        lua_pop(L_state, 1); // pop error message
                    }
                };

            return 0;
        }

        int Lua_GetPlayerSelectedItemId(lua_State* L) {
            int itemId = Framework::Inventory::Instance().GetSelectedItemId();
            // In your Inventory.cpp, -1 (unsigned -1) is empty. 
            // Let's ensure we return -1 to Lua if it's empty/max_uint
            if (itemId == (unsigned)-1) itemId = -1;

            lua_pushinteger(L, itemId);
            return 1; // Returns 1 value to Lua
        }

        // 2. Function to remove the item currently held (consumes it)
        int Lua_ConsumePlayerSelectedItem(lua_State* L) {
            auto& inv = Framework::Inventory::Instance();
            int idx = inv.GetSelectedIndex();

            // Clear the slot
            inv.ClearHotbarSlot(idx);

            return 0; // Returns nothing
        }

        static int Lua_LoadScene(lua_State* L)
        {
            int argc = lua_gettop(L);
            if (argc < 1 || !lua_isstring(L, 1))
                return luaL_error(L, "LoadScene(path[, additive]) expects at least a string");

            const char* path = lua_tostring(L, 1);
            bool additive = false;

            if (argc >= 2)
                additive = lua_toboolean(L, 2) != 0;

            // Use the global sceneManager instance
            sceneManager.LoadScene(path, additive);

            return 0; // no return values
        }
        
        // --------- Animation --------------
        // Helper to play an animation on an entity
        static int l_PlayEntityAnimation(lua_State* L) {
            // 1. Get Arguments: Entity ID and Animation Name
            Entity entity = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* animName = luaL_checkstring(L, 2);

            // 2. Check if entity has animation component
            if (ecsInterface.IsEntityValid(entity) && ecsInterface.HasComponent<AnimationProperties>(entity)) {
                auto& animProps = ecsInterface.GetComponent<AnimationProperties>(entity);

                // 3. Call the member function you wanted to use!
                animProps.PlayAnimation(animName);

                // Optional: Force restart if it's already playing
                animProps.currentTime = 0.0f;
            }
            else {
                std::cout << "[Lua Warning] PlayAnimation called on invalid entity or missing AnimationProperties: " << entity << std::endl;
            }
            return 0;
        }

        // Helper to configure animation properties
        static int l_SetAnimationSettings(lua_State* L) {
            // 1. Get Arguments
            Entity entity = static_cast<Entity>(luaL_checkinteger(L, 1));
            float speed = (float)luaL_checknumber(L, 2);
            bool loop = lua_toboolean(L, 3);
            bool playOnStart = lua_toboolean(L, 4);

            // 2. Check for Component
            if (ecsInterface.IsEntityValid(entity) && ecsInterface.HasComponent<AnimationProperties>(entity)) {
                auto& animProps = ecsInterface.GetComponent<AnimationProperties>(entity);

                // 3. Apply Settings
                animProps.animationSpeed = speed;
                animProps.bLoops = loop;
                animProps.bPlayOnStart = playOnStart;
            }
            return 0;
        }

        // Lua Call: bool CheckValidRecipe(table ingredientIDs)
        static int l_CheckValidRecipe(lua_State* L) {
            // 1. Read the Table from Lua (Argument 1)
            if (!lua_istable(L, 1)) {
                lua_pushboolean(L, false);
                return 1;
            }

            std::vector<int> cauldronIngredients;

            // Iterate over the Lua table (assuming it's a list {0, 1, ...})
            lua_pushnil(L);
            while (lua_next(L, 1) != 0) {
                // key is at index -2, value at -1
                if (lua_isnumber(L, -1)) {
                    cauldronIngredients.push_back((int)lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
            }

            // 2. Sort input ingredients to ensure order doesn't matter
            // (e.g. {0, 1} should equal {1, 0})
            std::sort(cauldronIngredients.begin(), cauldronIngredients.end());

            // 3. Check against Global Recipes
            const auto& allRecipes = Framework::GetGlobalRecipes();
            bool matchFound = false;

            for (const auto& recipe : allRecipes) {
                // Copy recipe ingredients to sort them for comparison
                std::vector<int> recipeIngredients = recipe.ingredientIDs;
                std::sort(recipeIngredients.begin(), recipeIngredients.end());

                if (cauldronIngredients.size() == recipeIngredients.size()) {
                    if (cauldronIngredients == recipeIngredients) {
                        matchFound = true;
                        break;
                    }
                }
            }

            // 4. Return result to Lua
            lua_pushboolean(L, matchFound);
            return 1;
        }

        // REGISTER IN SCRIPT SYSTEM
        // lua_register(L, "CheckValidRecipe", l_CheckValidRecipe);

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
        static int lua_GetEntityName(lua_State* L)
        {
            // 1. Get the Entity ID from the first argument passed in Lua
            // (Lua stack index 1 is the first argument)
            int entityID = (int)lua_tointeger(L, 1);
            auto& ecs = ecsInterface;
            // Optional: Check validity before getting name to prevent crashes
            // if (!ecs.IsEntityValid(entityID)) {
            //     lua_pushstring(L, "Invalid_Entity");
            //     return 1; 
            // }

            // 3. Get the name string
            std::string name = ecs.GetEntityName(entityID);

            // 4. Push the string onto the Lua stack
            lua_pushstring(L, name.c_str());

            // 5. Return 1 because we are returning one value (the string)
            return 1;
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
                std::cerr << "[Cook] Warning: failed to check timestamps � forcing recompile.\n";
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
            bool ok = skipped ? true : CompileArray<AudioBankMetadata>(json, out, desc, g_AudioBankMetadataProps, key);
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

        // Check if an entity has ItemComponent and if it is choppable
        static int lua_IsItemChoppable(lua_State* L)
        {
            // 1. Get Entity ID from Lua
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            // 2. Validate Entity
            if (!ecsInterface.IsEntityValid(e))
            {
                lua_pushboolean(L, false);
                return 1;
            }

            // 3. Check if it has the ItemComponent
            if (ecsInterface.HasComponent<ItemComponent>(e))
            {
                auto& item = ecsInterface.GetComponent<ItemComponent>(e);
                // 4. Return the bool value
                lua_pushboolean(L, item.isChoppable);
            }
            else
            {
                // Component doesn't exist, so it's definitely not choppable
                lua_pushboolean(L, false);
            }

            return 1; // Returning 1 value (boolean)
        }

        static uint64_t FileTimeToTicks(const std::filesystem::file_time_type& ft) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(ft.time_since_epoch()).count();
        }

        // [NEW] Helper to find the GUID string from a friendly name
        static int l_GetAnimationGUID(lua_State* L)
        {
            const char* name = luaL_checkstring(L, 1);
            // Ask AssetManager for the GUID of the animation with this name
            AssetHandle guid = AssetManager::Instance().FindGuid<AnimationProperties>(name);

            if (guid.id.empty()) {
                std::cout << "[Lua Warning] Animation Asset not found: " << name << std::endl;
            }

            lua_pushstring(L, guid.id.c_str());
            return 1; // Return the GUID string to Lua
        }

        // [NEW] Helper to swap the Animation Asset by GUID
        static int l_SetAnimationAsset(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            const char* guidStr = luaL_checkstring(L, 2);

            if (ecsInterface.IsEntityValid(e) && ecsInterface.HasComponent<AnimationProperties>(e))
            {
                auto& anim = ecsInterface.GetComponent<AnimationProperties>(e);
                AssetHandle newGuid(guidStr);

                // Only swap if it's different
                if (anim.guid.id != newGuid.id)
                {
                    anim.guid = newGuid;

                    // Load the new data
                    AnimationProperties* asset = AssetManager::Instance().Get<AnimationProperties>(newGuid);
                    if (asset)
                    {
                        // Ensure data is loaded
                        if (asset->skeleton.bones.empty()) {
                            AnimationLoader::loadAnimation(asset->path, *asset);
                        }

                        // Copy the new skeleton and clips
                        anim.skeleton = asset->skeleton;
                        anim.availableAnimations = asset->availableAnimations;
                        anim.globalInverseTransform = asset->globalInverseTransform;

                        // Reset Playback
                        anim.currentTime = 0.0f;
                        anim.currentAnimation = nullptr;
                        anim.StartDefaultAnimation();

                        // Resize matrices for safety
                        anim.finalBoneMatrices.resize(anim.skeleton.bones.size(), glm::mat4(1.0f));

                        // std::cout << "[Lua] Swapped Animation Asset to: " << asset->name << std::endl;
                    }
                }
            }
            return 0;
        }
        static int lua_SetScale(lua_State* L)
        {
            // 1. Get Entity ID
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            // 2. Get X, Y, Z scales
            // Lua arguments start from index 2
            float x = static_cast<float>(luaL_checknumber(L, 2));
            float y = static_cast<float>(luaL_checknumber(L, 3));
            float z = static_cast<float>(luaL_checknumber(L, 4));

            // 3. Validate Entity & Component
            if (!ecsInterface.IsEntityValid(e) || !ecsInterface.HasComponent<TransformComponent>(e))
            {
                return luaL_error(L, "SetScale: invalid entity %d or missing TransformComponent", (int)e);
            }

            // 4. Apply Scale
            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);
            tr.scale = glm::vec3(x, y, z);
            return 0;
        }

        // Helper to get the NAME of the currently selected item
        static int Lua_GetSelectedItemName(lua_State* L) {
            auto& inv = Framework::Inventory::Instance();
            int idx = inv.GetSelectedIndex();
            const auto& slot = inv.Get(idx);

            // Check if slot has an item and if the ID is safe
            if (slot.itemId >= 0 && slot.itemId < Framework::kItemCount) {
                // Access the array directly from ItemComponent.h
                lua_pushstring(L, Framework::kItemNames[slot.itemId]);
            }
            else {
                lua_pushnil(L); // Return nil if empty
            }
            return 1; // Returning 1 value (string or nil)
        }

        // In ScriptingSystem.cpp inside anonymous namespace

        static int lua_GetRotation(lua_State* L)
        {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));

            if (!ecsInterface.IsEntityValid(e) ||
                !ecsInterface.HasComponent<TransformComponent>(e))
            {
                return luaL_error(L, "GetRotation: entity %d has no TransformComponent", (int)e);
            }

            auto& tr = ecsInterface.GetComponent<TransformComponent>(e);
            lua_pushnumber(L, tr.rotation.x); // Pitch
            lua_pushnumber(L, tr.rotation.y); // Yaw
            lua_pushnumber(L, tr.rotation.z); // Roll
            return 3;
        }

        // In ScriptingSystem.cpp, inside the anonymous namespace

        static int lua_GetCameraYaw(lua_State* L)
        {
            // 1. Get the Graphics System
            auto* gfx = Graphics::GetInstance();
            if (!gfx) {
                lua_pushnumber(L, 0.0f);
                return 1;
            }

            // 2. Get the GAME CAMERA specifically (Index 0)
            // Index 1 would be the Scene/Editor camera
            Camera* cam = gfx->GetCameraByIndex(0);

            if (!cam) {
                lua_pushnumber(L, 0.0f);
                return 1;
            }

            // 3. Return the Yaw (Horizontal Rotation)
            lua_pushnumber(L, cam->yaw);
            return 1;
        }



    } // anonymous namespace

    // ---------------------------- class methods --------------------------------

    ScriptingSystem::ScriptingSystem() : L(nullptr) {}
    ScriptingSystem::~ScriptingSystem() { if (L) { lua_close(L); L = nullptr; } }

    void ScriptingSystem::Initialize() {
        // 1) Register ScriptComponent (your engine�s pattern)
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
        lua_register(L, "GetMousePosition", lua_GetMousePosition);
        lua_register(L, "DestroyEntity", l_DestroyEntity);
        lua_register(L, "IsMouseButtonDown", l_IsMouseButtonDown); 
        lua_register(L, "SetRotation", lua_SetRotation);
        lua_register(L, "SetMesh", lua_SetMesh);
        lua_register(L, "SetTexture", lua_SetTexture);
        lua_register(L, "SetAnimationSettings", l_SetAnimationSettings);
        lua_register(L, "LoadScene", Lua_LoadScene);

        //Components get & sets
        lua_register(L, "GetPosition", lua_GetPosition); // Transform get setter
        lua_register(L, "SetPosition", lua_SetPosition);

        lua_register(L, "GetTextComponent", lua_GetTextComponent); // Text component get/set
        lua_register(L, "SetTextComponent", lua_SetTextComponent);

        // Game utility
        lua_register(L, "IsItemChoppable", lua_IsItemChoppable);
        lua_register(L, "GetScore", lua_GetScore);
        lua_register(L, "AddScore", lua_AddScore);
        lua_register(L, "SetText", lua_SetText);

        lua_register(L, "DestroyEntity", Lua_DestroyEntity);
        lua_register(L, "GetItemName", Lua_GetItemName);
        lua_register(L, "RegisterButtonCallback",   Lua_RegisterButtonCallback);

        lua_register(L, "SpawnCauldronItem", l_spawnCauldronItem);
        lua_register(L, "CheckValidRecipe", l_CheckValidRecipe);
        lua_register(L, "GetEntityName", lua_GetEntityName);

        lua_register(L, "GetRotation", lua_GetRotation);
        lua_register(L, "GetCameraYaw", lua_GetCameraYaw);

        // Inventory Utility
        lua_register(L, "IsItemDestroyOnPick", Lua_IsItemDestroyOnPick);
        lua_register(L, "GetSelectedItem", Lua_GetPlayerSelectedItemId);
        lua_register(L, "ConsumeSelectedItem", Lua_ConsumePlayerSelectedItem);
        lua_register(L, "AddToInventory", Lua_AddToInventory);
        lua_register(L, "GetItemNameById", lua_GetItemNameById);
        lua_register(L, "GetSelectedItemName", Lua_GetSelectedItemName);
        lua_register(L, "SetInteractionText", l_SetInteractionText);



        // Engine utility
        lua_register(L, "FindEntityByName",     lua_FindEntityByName);
        lua_register(L, "GetEntitiesByTag",     lua_GetEntitiesByTag);
        lua_register(L, "RemoveTagFromEntity",  lua_RemoveTagFromEntity);
        lua_register(L, "EntityHasTag",         lua_EntityHasTag);
        lua_register(L, "IsEntityValid", lua_IsEntityValid);
        lua_register(L, "AddTagToEntity",       lua_AddTagToEntity);
        lua_register(L, "PlayEntityAnimation",  l_PlayEntityAnimation);
        lua_register(L, "SpawnPrefab",          lua_SpawnPrefab);
        lua_register(L, "FireEvent", lua_FireEvent); // Maybe add register if needed
        lua_register(L, "SetScriptVariable", lua_SetScriptVariable);

        // Engine States
        lua_register(L, "GameStateStart",       lua_GameStateStart);
        lua_register(L, "GameStateResume",      lua_GameStateResume);
        lua_register(L, "GameStatePaused",      lua_GameStatePaused);
        lua_register(L, "GameStateExit",        lua_GameStateExit);
        lua_register(L, "SetScale", lua_SetScale);
        //cauldron
        lua_register(L, "SetCameraLock", [](lua_State* L) -> int {
            // 1. Get argument (true/false)
            bool locked = lua_toboolean(L, 1);

            auto* input = InputHandler::GetInstance();
            if (input) {
                // "Locked" means rotation is disabled (for minigame)
                input->SetCameraRotationEnabled(!locked);
            }
            return 0;
            });

        lua_register(L, "SetGravity", [](lua_State* L) -> int {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            float gravity = static_cast<float>(luaL_checknumber(L, 2));

            if (ecsInterface.IsEntityValid(e) && ecsInterface.HasComponent<PhysicsProperties>(e))
            {
                auto& phys = ecsInterface.GetComponent<PhysicsProperties>(e);
                phys.gravityFactor = gravity;
                phys.startAwake = true; // Wake it up to apply change
            }
            return 0;
            });

        lua_register(L, "SetAnimationMode", [](lua_State* L) -> int {
            // Arg 1: Entity ID
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            // Arg 2: Mode (0=Normal, 1=Stirring, 2=Manual)
            int mode = static_cast<int>(luaL_checkinteger(L, 2));

            // Validate and Apply
            if (ecsInterface.IsEntityValid(e) && ecsInterface.HasComponent<AnimationProperties>(e))
            {
                auto& anim = ecsInterface.GetComponent<AnimationProperties>(e);
                anim.updateMode = mode;
                // std::cout << "[Scripting] Set Animation Mode to " << mode << " for Entity " << e << std::endl;
            }
            return 0;
            });

        lua_register(L, "GetAnimationGUID", l_GetAnimationGUID);
        lua_register(L, "SetAnimationAsset", l_SetAnimationAsset);
        // Navmesh
        lua_register(L, "BuildNavMesh", lua_BuildNavMesh);
        lua_register(L, "PathfindTo", [](lua_State* L) -> int 
        {
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

        lua_register(L, "PlayEntitySound", [](lua_State* L) -> int {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            if (g_AudioSystem) {
                g_AudioSystem->PlayEntitySound(e);
            }
            return 0;
            });

        lua_register(L, "SetEntityVolume", [](lua_State* L) -> int {
            Entity e = static_cast<Entity>(luaL_checkinteger(L, 1));
            float volume = (float)luaL_checknumber(L, 2);

            if (Framework::g_AudioSystem) {
                Framework::g_AudioSystem->SetEntityVolume(e, volume);
            }
            return 0;
            });

        // Create Sound table with playback functions
        lua_newtable(L);
        lua_pushcfunction(L, lua_AudioPlay);      lua_setfield(L, -2, "Play");
        lua_pushcfunction(L, lua_AudioStop);      lua_setfield(L, -2, "Stop");
        lua_pushcfunction(L, lua_AudioPause);     lua_setfield(L, -2, "Pause");
        lua_pushcfunction(L, lua_AudioResume);    lua_setfield(L, -2, "Resume");
        lua_pushcfunction(L, lua_AudioSetVolume); lua_setfield(L, -2, "SetVolume");
        lua_pushcfunction(L, lua_AudioPlay3D);    lua_setfield(L, -2, "Play3D");
        lua_setglobal(L, "Sound");

        std::cout << "[ScriptingSystem] Initialized (Lua VM ready)\n";
    }   

    void ScriptingSystem::Update(float dt)
    {
        // CREATE A COPY of the entity set to iterate over
        std::vector<Entity> entitiesToProcess;
        entitiesToProcess.reserve(mEntities.size());
        for (Entity e : mEntities) {
            entitiesToProcess.push_back(e);
        }   

        // NOW iterate over the COPY, not the original set
        for (Entity e : entitiesToProcess) {
            // Add safety check: entity might have been destroyed by a previous script
            if (!ecsInterface.IsEntityValid(e)) {
                continue;
            }

            if (!ecsInterface.HasComponent<ScriptProperties>(e)) continue;
            auto& comp = ecsInterface.GetComponent<ScriptProperties>(e);

            for (auto& sc : comp.scripts) {
                if (sc.scriptPath.empty()) continue;

                // ----- AUTO HOT RELOAD POLLING -----
                if (sc.hotReload) {
                    std::error_code ec;
                    auto ft = std::filesystem::last_write_time(sc.scriptPath, ec);
                    if (ec) {
                        static std::unordered_set<std::string> alreadyLogged;
                        if (!alreadyLogged.count(sc.scriptPath)) {
                            std::cerr << "[HotReload] last_write_time failed for "
                                << sc.scriptPath << ": " << ec.message() << "\n";
                            alreadyLogged.insert(sc.scriptPath);
                        }
                        sc.hotReload = false;
                    }
                    else {
                        auto nowTicks = FileTimeToTicks(ft);
                        if (nowTicks != sc.lastWriteTicks) {
                            std::cout << "[HotReload] " << sc.scriptPath << "\n";
                            if (LoadScriptTable(sc)) {
                                lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                                lua_pushinteger(L, static_cast<lua_Integer>(e));
                                lua_setfield(L, -2, "entity");
                                lua_pop(L, 1);
                                CallHook(sc.tableRef, "on_init");
                                sc.initialized = true;
                            }
                            sc.lastWriteTicks = nowTicks;
                        }
                    }
                }

                if (sc.tableRef == LUA_NOREF || sc.tableRef == -1)
                {
                    if (!LoadScriptTable(sc)) continue;
                    lua_rawgeti(L, LUA_REGISTRYINDEX, sc.tableRef);
                    lua_pushinteger(L, static_cast<lua_Integer>(e));
                    lua_setfield(L, -2, "entity");
                    lua_pop(L, 1);
                    sc.initialized = false;
                }

                if (!sc.initialized) {
                    CallHook(sc.tableRef, "on_init");
                    sc.initialized = true;
                }

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
