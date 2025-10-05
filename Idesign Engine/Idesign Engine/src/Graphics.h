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

//
////FILES
#include "System.h"
#include <InputHandler.h>
#include <FontSystem.h>
#include "AssetManager.h"
#include <ComponentList.h>
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "Lighting.h"
#include "MeshLoader.h"
#include "TextureLoader.h"

#include "AssetManager.h"
#include "WindowConfigProperties.h"
#include "MeshProperties.h"
#include "TextProperties.h"

namespace Framework {
    class ImguiManager;
}


namespace Framework {

	class Graphics : public ISystem
	{
	public:
		Graphics();
		~Graphics();

		void Initialize() override;
		void Update(float deltaTime) override;
		std::string GetName() override;
        static GLuint CreateFramebuffer(int width, int height, GLuint& texture, GLuint& rbo_fb);
        void RequestPickUV(float u, float v);
        //JAY
        // returns picked entity id (>=0) and clears it; -1 if none
        int GetAndClearPickedEntity();
		////linking
		//GraphicsWindows* graphicWindows;
		//InputHandler* InputHandlerInstance;
		//FontSystem fontSystem; // Define the instance

        //Getter for windows for external systems.
        GLFWwindow* GetWindow();
        void SetImguiManager(ImguiManager* manager);

        Camera* getCamera();

    private:
        static void framebuffer_size_callback(GLFWwindow*, int, int);
        static void mouse_callback(GLFWwindow*, double, double);
        

        void process_input(float dt);
        void renderQuad();

        // window & timing
        GLFWwindow* window = nullptr;
        float lastTime = 0.0f;

        // camera & movement
        Camera camera{ {0.f, 1.f, 3.f} };
        static bool mouseCaptured;
        bool groundMode = true;

        // scene content
        Terrain terrain{ 128, 1.0f };
        Mesh cube;
        Mesh cone;
        Mesh plane;
        Mesh cylinder;
        Mesh triangle;
        MeshProperties mesh;
        GLuint tex = 0;

        // shaders
        ShaderProgram terrainShader{ "Assets/shaders/terrainvert.glsl", "Assets/shaders/terrainfrag.glsl" };
        ShaderProgram meshShader{ "Assets/shaders/meshvert.glsl", "Assets/shaders/meshfrag.glsl" };
        ShaderProgram blurShader{ "Assets/shaders/postprocess.glsl", "Assets/shaders/blur.glsl" };
        ShaderProgram finalShader{ "Assets/shaders/postprocess.glsl", "Assets/shaders/final.glsl" };
        ShaderProgram debugShader{ "Assets/shaders/debugVertexShader.glsl", "Assets/shaders/debugFragmentShader.glsl" };
        // lighting
        DirectionalLightCPU sun;
        bool hasSun = true;
        std::vector<PointLightCPU> pointLights;
        std::vector<SpotLightCPU>  spotLights;


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
        //void DrawDebugBox(const TransformComponent& transform, const glm::vec3& color);
        // Draws a wireframe sphere using the transform's position and scale as radius
        void DrawDebugMesh(Mesh* mesh, const glm::vec3& color, const glm::mat4& model, const glm::mat4& projection, const glm::mat4& view);
    };

}
#endif // !_GRAPHICS_H_
