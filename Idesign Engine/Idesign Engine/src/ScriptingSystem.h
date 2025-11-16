#pragma once
#include "System.h"
#include <string>
#include "ScriptProperties.h"

struct lua_State;
struct ScriptComponent;
struct ScriptInstance;

namespace Framework {

    class Graphics; // fwd-declare

    class ScriptingSystem : public ISystem {
    public:
        ScriptingSystem();
        ~ScriptingSystem() override;

        void Initialize() override;
        void Update(float dt) override;
        std::string GetName() override;

        // NEW: allow main.cpp to wire Graphics into the system
        void SetGraphicsSystem(Graphics* g) { graphics_ = g; }
        Graphics* GetGraphicsSystem() const { return graphics_; }

        // OPTIONAL helper to spawn a script-driven entity
        Entity CreateScriptEntity(const std::string& path);

        void HotReloadScript(const std::string& changedPath);

        lua_State* GetLuaState() const { return L; }


    private:
        lua_State* L = nullptr;
        Graphics* graphics_ = nullptr;

        bool LoadScriptTable(ScriptEntry& inst);
        bool CallHook(int tableRef, const char* fn, float arg = 0.0f);
    };

} // namespace Framework
