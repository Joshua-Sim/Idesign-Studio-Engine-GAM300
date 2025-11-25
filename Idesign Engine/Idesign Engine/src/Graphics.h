///////////////////////////////////////////////////////////////////////////////
///
///	@file Graphics.h
/// 
/// @brief Declaration of graphics.cpp
///	
///	@Authors: Victor lee
///	Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once //Makes sure this header is only included once
#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

////FILES
#include "System.h"
#include <InputHandler.h>
#include <FontSystem.h>
#include "AssetManager.h"
#include <ComponentList.h>
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "ShaderProgram.h"
#include "Inventory.h"

#include "WindowConfigProperties.h"
#include "MeshProperties.h"
#include "TextProperties.h"
#include "Skybox.h"

extern Terrain      terrain;
extern const int    TerrainSize;
extern const float  TerrainScale;

namespace Framework 
{
    class ImguiManager;
}

namespace Framework 
{
    class Graphics : public ISystem
    {
    public:
        Graphics();
        ~Graphics();

        void Initialize() override;
        
        void Update(float deltaTime) override;
        std::string GetName() override;
        static GLuint CreateFramebuffer(int width, int height, GLuint& texture, GLuint& rbo_fb);
        void RequestPickUV(float u, float v); //main viewport (0)
        void RequestPickUVSecondary(float u, float v);  // secondary viewport (1)
        void SetPickViewIndex(int index);

        ////linking
        //GraphicsWindows* graphicWindows;
        //InputHandler* InputHandlerInstance;
        //FontSystem fontSystem; // Define the instance

        //Getter for windows for external systems.
        GLFWwindow* GetWindow();
        void SetImguiManager(ImguiManager* manager);

        Camera* getCamera();

        void MoveCameraFromLua(const std::string& dir, float dt, bool groundMode);
        void NudgeLightFromLua(const std::string& axis, float amount);
        void ToggleMainLightFromLua();
        void SetHDRFromLua(float value);
        void SetBloomFromLua(float value);
        void SetGroundModeFromLua(bool gm);
        bool GetGroundModeFromLua() const;
        void SetScriptControlEnabled(bool on);
        bool IsScriptControlEnabled() const;

        int  ConsumeLastPickedId();
        glm::mat4 GetViewMatrix(int viewIdx) const;
        glm::mat4 GetProjectionMatrix(int viewIdx, float w, float h) const;
        Camera* GetCameraByIndex(int viewIdx);
        void OnResize(int w, int h);

        // To allow cam to move again when swap
        void SetGroundMode(bool state);

        void SetMouseTargetView(int view) { m_mouseTargetView = (view <= 0) ? 0 : 1; }
        int  GetMouseTargetView() const;
        void CaptureMouse(bool on);

        // Static flag so we can access it anywhere without an instance
        static bool s_RenderDebugColliders;


        //global assecs for systems
        static Graphics* GetInstance() { return s_instance; }

        //Function called by Lua to lock/unlock
        void SetCameraRotationEnabled(bool enabled);
        bool IsCameraRotationEnabled() const { return m_cameraRotationEnabled; }

        //HOTBAR
        void DrawUIRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec3& color, float alpha);
        GLuint GetOrLoadIconTex(const char* path);
        GLuint GetOrLoadIconTexFromItemId(int itemId);

    private:

        static Graphics* s_instance;
        bool m_cameraRotationEnabled = true;

        static void framebuffer_size_callback(GLFWwindow*, int, int);
        static void mouse_callback(GLFWwindow*, double, double);

        glm::vec2 pendingPickUV0{ -1.f, -1.f };  // main viewport
        glm::vec2 pendingPickUV1{ -1.f, -1.f };  // secondary viewport
        int pickViewIndex = 0; // 0 = Grounded, 1 = debug
        void process_input(float dt);
        void renderQuad();
        void RenderViewToFramebuffer(Camera& viewCam, GLuint imguiTargetFBO,
            int screenW, int screenH, int viewIndex);

        bool scriptControl_ = false;
        int m_mouseTargetView = 0; // 0 = main/game view, 1 = secondary/scene view
        // window & timing
        GLFWwindow* window = nullptr;
        float lastTime = 0.0f;

        // camera & movement
       Camera camera{ {0.f, 1.f, 3.f} };
        static bool mouseCaptured;
        static bool groundMode;

        enum class CameraMode
        {
            Scene,
            Game
        };

        Camera sceneCamera{ {0.f, 3.f, 8.f} }; // Free-fly editor camera
        Camera gameCamera{ {-33, 7.631, 14} }; // Gameplay camera
        CameraMode activeCameraMode;

        Camera& cam = GetActiveCamera();

        Camera& GetActiveCamera()
        {
            return (activeCameraMode == CameraMode::Scene) ? sceneCamera : gameCamera;
        }
        void SetCameraMode(CameraMode mode) { activeCameraMode = mode; }
        CameraMode GetCameraMode() const { return activeCameraMode; }

        // ========================== //
        //   END OF CAMERA FUNCTIONS //

        // scene content
        //Terrain terrain{ 128, 1.0f };
        Mesh cube;
        Mesh cone;
        Mesh plane;
        Mesh cylinder;
        Mesh triangle;
        MeshProperties mesh;
        GLuint tex = 0;

        //sky
        Skybox m_skybox; 

        // shaders
        ShaderProgram terrainShader{ "Assets/shaders/terrainvert.glsl", "Assets/shaders/terrainfrag.glsl" };
        ShaderProgram meshShader{ "Assets/shaders/meshvert.glsl", "Assets/shaders/meshfrag.glsl" };
        ShaderProgram blurShader{ "Assets/shaders/postprocess.glsl", "Assets/shaders/blur.glsl" };
        ShaderProgram finalShader{ "Assets/shaders/postprocess.glsl", "Assets/shaders/final.glsl" };
        ShaderProgram debugShader{ "Assets/shaders/debugVertexShader.glsl", "Assets/shaders/debugFragmentShader.glsl" };
        ShaderProgram skinnedShader{ "Assets/shaders/SkinnedVert.glsl", "Assets/shaders/meshfrag.glsl" };
        ShaderProgram shadowShader{ "Assets/shaders/shadowvert.glsl", "Assets/shaders/shadowfrag.glsl" };
        ShaderProgram pointShadowShader{ "Assets/shaders/point_shadow_vert.glsl", "Assets/shaders/point_shadow_frag.glsl" };
        ShaderProgram uiShader{ "Assets/shaders/Shader2dvert.glsl", "Assets/shaders/Shader2dfrag.glsl" };
        //shadow
        GLuint m_shadowFBO = 0;
        GLuint m_shadowDepthTexture = 0;
        glm::mat4 m_lightSpaceMatrix;
        const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
        const unsigned int MAX_POINT_LIGHTS = 8;
        const unsigned int MAX_SPOT_LIGHTS = 4;

        // ADD HDR & BLOOM VARIABLES
        float hdrExposure = 1.0f;
        float bloomStrength = 1.0f;

        GLuint hdrFBO = 0;
        GLuint colorBuffer = 0;
        GLuint rboDepth = 0;

        GLuint pingpongFBO[2] = { 0, 0 };
        GLuint pingpongColorbuffers[2] = { 0, 0 };

        GLuint quadVAO = 0;
        GLuint quadVBO = 0;

        GLuint m_debugVao = 0;
        GLuint m_debugVbo = 0;

        ImguiManager* m_imguiManager = nullptr;
        // jay objpick
        GLuint pickFBO[2] = { 0,0 };
        GLuint pickColor[2] = { 0,0 };   // GL_RGB8 color IDs
        GLuint pickDepth[2] = { 0,0 };   // GL_DEPTH24 (shared per FBO)
        int    pickWrite = 0;                // current we render into
        int    pickRead = 1;                // previous we read from
        ShaderProgram pickShader{ "Assets/shaders/pickvert.glsl", "Assets/shaders/pickfrag.glsl" };
        glm::vec2 pendingPickUV = { -1.0f,-1.0f };
        int lastPickedId = -1;

        std::map<DebugShapeType, Mesh*> m_debugMeshes;

        //draw debug shapes
        void DrawDebugBox(const TransformComponent& transform, const glm::vec3& color);
        // Draws a wireframe sphere using the transform's position and scale as radius
        void DrawDebugMesh(Mesh* mesh, const glm::vec3& color, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view);

        // Add these new members for the FPS counter
        double m_fpsLastTime;
        int m_frameCount;

        // For screen resizing
        int m_fbW = 0, m_fbH = 0;      // current render size (window framebuffer)
        int m_pickW = 0, m_pickH = 0;  // current size for pick FBOs

        void GatherLights(
            DirectionalLightCPU& outSun,
            bool& outHasSun,
            std::vector<PointLightCPU>& outPointLights,
            std::vector<SpotLightCPU>& outSpotLights);

        void InitializeFramebuffers(int width, int height);

        void RenderView(
            Camera& cam,
            int viewIndex,
            int width, int height,
            const DirectionalLightCPU& sun, bool hasSun,
            const std::vector<PointLightCPU>& points,
            const std::vector<SpotLightCPU>& spots
        );

        // Add these variables
        bool m_resizePending = false;
        int m_newWidth = 0;
        int m_newHeight = 0;

        GLuint m_WhiteTex = 0; // 1x1 white for solid-colored rects

        // helpers
        //std::array<x, 10> HotbarSlot{};
        void DrawHotbar(int screenW, int screenH);
        void DrawUISprite(GLuint tex, const glm::vec2& pos, const glm::vec2& size, float alpha = 1.0f);
        static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

    };
    
}
#endif // !_GRAPHICS_H_