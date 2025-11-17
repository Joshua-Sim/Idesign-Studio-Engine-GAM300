#pragma once
#include "System.h"
#include <string>

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

        // for directory

        static inline std::string s_ProjectRoot;  

        static void SetProjectRoot(const std::string& root);

        static std::string GetProjectRoot();

        void CallEntityFunction(Entity entity, const std::string & functionName, Entity arg1, Entity arg2);

    private:
        lua_State* L = nullptr;
        Graphics* graphics_ = nullptr;


        bool LoadScriptTable(ScriptInstance& inst);
        bool CallHook(int tableRef, const char* fn, float arg = 0.0f);
    };

} // namespace Framework
