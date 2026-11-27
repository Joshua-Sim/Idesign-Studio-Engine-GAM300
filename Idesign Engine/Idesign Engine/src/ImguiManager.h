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
#include "GraphicsShader.h"
#include "InputHandler.h"
#include "FontSystem.h"
#include "AssetManager.h"
#include "ComponentList.h"
#include "PropertyWidget.h"
#include "MessageSystem.h"
#include <Graphics.h>
#include "TextureLoaderDDS.h"
#include "EnginePath.h"

namespace Framework 
{
	class Graphics;  // Forward declaration

	//Console Entry Structure
	struct ConsoleEntry 
	{
		std::string timestamp;
		std::string category;
		std::string message;
		ConsoleLogMessage::LogLevel level;
	};

	class ImguiManager : public ISystem 
	{
	public:
		ImguiManager();
		~ImguiManager();

		void Initialize();
		void Update(float deltaTime);
		UpdateStage GetUpdateStage() const override { return UpdateStage::Frame; } // draw + swap once per frame
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
		void RenderEntityMenu();
		void RenderGizmoPanel();
		void RenderGizmoForSelected(const glm::mat4& view, const glm::mat4& proj,
			const ImVec2& viewportTopLeft, const ImVec2& viewportSize);
		void ResizeViewFBOs(int w, int h);
		int GetFboWidth() const { return m_fboW; }
		int GetFboHeight() const { return m_fboH; }


		int GetMainViewIndex() const;

		Graphics* m_graphicsSystem; // Pointer to store the connection
		bool m_autoReload = true;
		int m_gizmoOperation = 0; // 0=Translate, 1=Rotate, 2=Scale
		int m_gizmoMode = 0; // 0=Local, 1=World
		bool m_gizmoEnabled = true;

		bool m_showEditor = false;

		//Console Methods 
		void RenderConsole();
		static void Log(const std::string& message, const std::string& category = "APP");
		static void LogWarning(const std::string& message, const std::string& category = "APP");
		static void LogError(const std::string& message, const std::string& category = "APP");
		static void LogSuccess(const std::string& message, const std::string& category = "APP");

		// Asset Panel
		enum AssetTabType { None, Mesh,Animation, Texture, Audio, Prefab, Scene, Scripts };

		struct AssetTab
		{
			AssetTabType type;
			std::string label;
		};
		std::vector<AssetTab> openTabs;

		struct AssetFolder 
		{
			std::string name;
			AssetTabType type;
			std::vector<AssetFolder> children;
		};

		std::vector<AssetFolder> assetTree = 
		{
			{ "Assets", AssetTabType::None, 
				{
					{ "Meshes",		AssetTabType::Mesh, {} },
					{ "Animations", AssetTabType::Animation, {} },
					{ "Textures",	AssetTabType::Texture, {} },
					{ "Audio",		AssetTabType::Audio, {} },
					{ "Prefabs",	AssetTabType::Prefab, {} },
					{ "Scenes",		AssetTabType::Scene, {} },
					{ "Scripts",	AssetTabType::Scripts, {} }
				}
			}
		};

		void RenderFolderTree(const AssetFolder& folder);
		void RenderAssetPanel();
		void RenderMeshAssets();
		void RenderTextureAssets();
		void RenderAudioAssets();
		void RenderPrefabAssets();
		void RenderSceneAssets();
		void RenderScriptAssets();
		void RenderAnimationAssets();
		bool IsView0Visible() const { return m_isView0Visible; }
		bool IsView1Visible() const { return m_isView1Visible; }
	private:
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
		int m_fboW;
		int m_fboH;
		bool m_isView0Visible = true;
		bool m_isView1Visible = false;
	};
}