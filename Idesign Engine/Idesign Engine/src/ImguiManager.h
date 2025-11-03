#pragma once
#include <glew.h>
#include <vec2.hpp>
#include <array>
#include <iostream>
#include <sstream>
#include <iomanip> 
#include <gtc/constants.hpp>
#include <cstdlib>
#include <unordered_map> 
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>  

//FILES
#include "System.h"
#include <GraphicsShader.h>
#include <InputHandler.h>
#include <FontSystem.h>
#include "AssetManager.h"
#include <ComponentList.h>
#include "AudioLoader.h"
#include "UndoRedoSystem.h"
#include "PropertyWidget.h"

namespace Framework {
	class ImguiManager : public ISystem {
	public:
		ImguiManager();
		~ImguiManager();

		void Initialize();
		void Update(float deltaTime);
		std::string GetName() override;

		void SetGraphicsSystem(Framework::Graphics* graphicsSystem); // New "setter" function
		GLuint GetGameFramebuffer();
		GLuint GetGameFramebuffer(int viewIndex);
		GLuint GetGameTexture(int viewIndex);
		std::string OpenFileDialog();
		std::string SaveFileDialog();
		void RenderErrorPopup();
		void RenderMenuBar();
		void RenderMeshAssets();
		void RenderAssetBar();
		void RenderAudioManager();
		void RenderEntityMenu();
		int GetMainViewIndex() const;
	private:
		Framework::Graphics* m_graphicsSystem = nullptr; // Pointer to store the connection
	};
}

