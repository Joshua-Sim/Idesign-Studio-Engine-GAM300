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
#include <deque>

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
#include "MessageSystem.h"

namespace Framework {
	class Graphics;  // Forward declaration
}

namespace Framework {

	//Console Entry Structure
	struct ConsoleEntry {
		std::string timestamp;
		std::string category;
		std::string message;
		ConsoleLogMessage::LogLevel level;
	};

	class ImguiManager : public ISystem {
	public:
		ImguiManager();
		~ImguiManager();

		void Initialize();
		void Update(float deltaTime);
		std::string GetName() override;

		void SetGraphicsSystem(Graphics* graphicsSystem); // New "setter" function
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
		//Console Methods 
		void RenderConsole();
		static void Log(const std::string& message, const std::string& category = "APP");
		static void LogWarning(const std::string& message, const std::string& category = "APP");
		static void LogError(const std::string& message, const std::string& category = "APP");
		static void LogSuccess(const std::string& message, const std::string& category = "APP");


		Graphics* m_graphicsSystem = nullptr; // Pointer to store the connection

		//Console members 
		void OnConsoleLog(const ConsoleLogMessage& msg);
		glm::vec4 GetColorForLevel(ConsoleLogMessage::LogLevel level);

		std::deque<ConsoleEntry> consoleBuffer;
		size_t maxBufferSize = 1000;
		bool autoScroll = true;
		char filterBuffer[256] = "";
		bool showInfo = true;
		bool showWarning = true;
		bool showError = true;
		bool showSuccess = true;
		bool showConsole = true; 
		
		static ImguiManager* instance; 
	};
}

