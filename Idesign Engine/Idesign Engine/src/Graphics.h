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
#include <PhysicsSystem.h>
#include <FontSystem.h>
#include "AssetManager.h"
#include <ComponentList.h>
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "Lighting.h"
#include "MeshLoader.h"
#include "TextureLoader.h"

namespace Framework {

	class Graphics : public ISystem
	{
	public:
		Graphics();
		~Graphics();

		void Initialize() override;
		void Update(float deltaTime) override;
		std::string GetName() override;

		////linking
		//GraphicsWindows* graphicWindows;
		//InputHandler* InputHandlerInstance;
		//FontSystem fontSystem; // Define the instance

		// Sorting Layer Variables
		unsigned int CurrentSize = -1;
		std::vector<Entity> sortedEntities;

    private:
        static void framebuffer_size_callback(GLFWwindow*, int, int);
        static void mouse_callback(GLFWwindow*, double, double);

        void process_input(float dt);

        // window & timing
        GLFWwindow* window = nullptr;
        int width = 1280, height = 720;
        float lastTime = 0.0f;

        // camera & movement
        Camera camera{ {0.f, 1.f, 3.f} };
        static bool mouseCaptured;
        bool groundMode = true;

        // scene content
        Terrain terrain{ 128, 1.0f };
        Mesh cube;
        MeshComponent mesh;
        GLuint tex = 0;

        // shaders
        ShaderProgram terrainShader{ "Assets/shaders/terrainvert.glsl", "Assets/shaders/terrainfrag.glsl" };
        ShaderProgram meshShader{ "Assets/shaders/meshvert.glsl", "Assets/shaders/meshfrag.glsl" };

        // lighting
        DirectionalLightCPU sun;
        bool hasSun = true;
        std::vector<PointLightCPU> pointLights;
        std::vector<SpotLightCPU>  spotLights;
	};

}
#endif // !_GRAPHICS_H_
