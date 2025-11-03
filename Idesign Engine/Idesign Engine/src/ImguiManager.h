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
		void DrawHotReloadPrompt();
		void RenderErrorPopup();
		void RenderMenuBar();
		void RenderMeshAssets();
		void RenderAssetBar();
		void RenderAudioManager();
		void RenderEntityMenu();
		void RenderGizmoPanel();
		void RenderGizmoForSelected(const glm::mat4& view, const glm::mat4& proj,
			const ImVec2& viewportTopLeft, const ImVec2& viewportSize);

		int GetMainViewIndex() const;
	private:
		Framework::Graphics* m_graphicsSystem = nullptr; // Pointer to store the connection
		bool m_autoReload = true;
		int m_gizmoOperation = 0; // 0=Translate, 1=Rotate, 2=Scale
		int m_gizmoMode = 0; // 0=Local, 1=World
		bool m_gizmoEnabled = true;
	};
}

