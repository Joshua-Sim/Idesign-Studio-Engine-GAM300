#include "pch.h"
//#include <windows.h>
//#include <commdlg.h>
#include "Graphics.h"

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//#include "stb_image_resize2.h"
//#include <InputHandler.h>
//#include "ComponentList.h"
//#include "FontSystem.h"
//#include "Coordinator.h"
//#include <algorithm>
//
//#include "imgui.h"
//#include "imgui_impl_glfw.h"
//#include "imgui_impl_opengl3.h"
//#include "EngineState.h"
//#include "AssetManager.h"
//#include "TextureAsset.h"
//#include "Debugger.h"
#include<GameHUD.h>
#include <glew.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <iostream>
#include "Core.h"
#include "ShaderProgram.h"
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "NavMeshAgentProperties.h"
#include "ImguiManager.h"
#include "Coordinator.h"
#include "ComponentList.h"
#include <NavMeshProperties.h>
#include "FontSystem.h"
#include "PrefabProperties.h"
#include "PrefabComponent.h"
#include "TextureLoaderDDS.h"
#include "PhysicsSystem.h"
#include "AnimationLoader.h"
#include "EngineState.h"

namespace Framework
{


    // ---------- small internal HUD renderer (2D quads) ----------

    namespace
    {
        GLuint g_HudVAO = 0;
        GLuint g_HudVBO = 0;
        GLuint g_HudProgram = 0;
        GLint  g_HudColorLoc = -1;

        GLuint CompileShader(GLenum type, const char* src)
        {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char log[512];
                glGetShaderInfoLog(s, 512, nullptr, log);
                std::cout << "HUD shader compile error:\n" << log << std::endl;
            }
            return s;
        }

        void InitHudRenderer()
        {
            if (g_HudProgram != 0)
                return;

            const char* vsSrc = R"(
                #version 430 core
                layout (location = 0) in vec2 aPos;
                void main()
                {
                    gl_Position = vec4(aPos, 0.0, 1.0);
                }
            )";

            const char* fsSrc = R"(
                #version 430 core
                out vec4 FragColor;
                uniform vec4 uColor;
                void main()
                {
                    FragColor = uColor;
                }
            )";

            GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
            GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);

            g_HudProgram = glCreateProgram();
            glAttachShader(g_HudProgram, vs);
            glAttachShader(g_HudProgram, fs);
            glLinkProgram(g_HudProgram);
            glDeleteShader(vs);
            glDeleteShader(fs);

            g_HudColorLoc = glGetUniformLocation(g_HudProgram, "uColor");

            glGenVertexArrays(1, &g_HudVAO);
            glGenBuffers(1, &g_HudVBO);

            glBindVertexArray(g_HudVAO);
            glBindBuffer(GL_ARRAY_BUFFER, g_HudVBO);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glBindVertexArray(0);
        }

        // Draw a filled rectangle in pixel coords (x,y from bottom-left)
        void DrawHudBox(float x, float y, float w, float h,
            int screenW, int screenH,
            const glm::vec4& color)
        {
            InitHudRenderer();

            // pixel -> NDC
            float left = (x / screenW) * 2.0f - 1.0f;
            float right = ((x + w) / screenW) * 2.0f - 1.0f;
            float bottom = (y / screenH) * 2.0f - 1.0f;
            float top = ((y + h) / screenH) * 2.0f - 1.0f;

            float verts[] = {
                left,  top,
                left,  bottom,
                right, top,
                right, bottom
            };

            GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(g_HudProgram);
            glUniform4f(g_HudColorLoc, color.r, color.g, color.b, color.a);

            glBindVertexArray(g_HudVAO);
            glBindBuffer(GL_ARRAY_BUFFER, g_HudVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glBindVertexArray(0);
            glUseProgram(0);

            if (!blendWasEnabled) glDisable(GL_BLEND);
            if (depthWasEnabled)  glEnable(GL_DEPTH_TEST);
        }
    } // anonymous namespace

    // ---------- GameHUD methods ----------

    GameHUD::GameHUD()
        : m_window(nullptr)
        , m_lastLeftDown(false)
    {
    }

    void GameHUD::Initialize(GLFWwindow* window)
    {
        m_window = window;
    }

    void GameHUD::Draw(FontSystem& fontSystem, int screenW, int screenH)
    {
        if (!m_window)
            return;

        // If the game is already running, don't show the overlay at all
        if (engineState.IsPlay())
            return;

        // ---------- mouse ----------
        double mx, my;
        glfwGetCursorPos(m_window, &mx, &my);
        float mouseX = static_cast<float>(mx);
        float mouseY = static_cast<float>(my);
        float uiY = static_cast<float>(screenH) - mouseY; // flip Y

        int leftState = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
        bool leftDown = (leftState == GLFW_PRESS);
        bool clicked = leftDown && !m_lastLeftDown;
        m_lastLeftDown = leftDown;

        // ---------- full-screen dark overlay ----------
        DrawHudBox(
            0.0f, 0.0f,
            (float)screenW, (float)screenH,
            screenW, screenH,
            glm::vec4(0.25f, 0.88f, 0.82f, 1.0f)   // almost opaque black
        );

        // ---------- buttons layout ----------
        float btnW = 260.0f;
        float btnH = 60.0f;
        float spacing = 20.0f;

        float centerX = screenW * 0.5f;
        float centerY = screenH * 0.5f;

        // PLAY button above
        float playBx = centerX - btnW * 0.5f;
        float playBy = centerY + 0.5f * (btnH + spacing);

        // QUIT button below
        float quitBx = centerX - btnW * 0.5f;
        float quitBy = centerY - 0.5f * (btnH + spacing);

        // ---------- draw PLAY button ----------
        DrawHudBox(playBx, playBy, btnW, btnH, screenW, screenH,
            glm::vec4(0.2f, 0.2f, 0.0f, 0.9f)); // dark yellowish
        DrawHudBox(playBx - 2.0f, playBy - 2.0f,
            btnW + 4.0f, btnH + 4.0f,
            screenW, screenH,
            glm::vec4(1.0f, 1.0f, 0.0f, 0.4f)); // border glow

        const char* playLabel = "PLAY";
        float playScale = 1.1f;
        float charW = 14.0f * playScale;
        float playWidth = charW * std::strlen(playLabel);
        float playTextX = playBx + 0.5f * (btnW - playWidth);
        float playTextY = playBy + 0.5f * btnH - 8.0f * playScale;

        fontSystem.RenderText2D(
            playLabel,
            playTextX, playTextY,
            playScale,
            glm::vec3(1.0f, 1.0f, 0.5f),
            (float)screenW,
            (float)screenH
        );

        bool overPlay =
            mouseX >= playBx && mouseX <= playBx + btnW &&
            uiY >= playBy && uiY <= playBy + btnH;

        // ---------- draw QUIT button ----------
        DrawHudBox(quitBx, quitBy, btnW, btnH, screenW, screenH,
            glm::vec4(0.2f, 0.0f, 0.0f, 0.9f)); // dark reddish
        DrawHudBox(quitBx - 2.0f, quitBy - 2.0f,
            btnW + 4.0f, btnH + 4.0f,
            screenW, screenH,
            glm::vec4(1.0f, 0.2f, 0.0f, 0.4f)); // border glow

        const char* quitLabel = "QUIT";
        float quitScale = 1.1f;
        float quitWidth = charW * std::strlen(quitLabel);
        float quitTextX = quitBx + 0.5f * (btnW - quitWidth);
        float quitTextY = quitBy + 0.5f * btnH - 8.0f * quitScale;

        fontSystem.RenderText2D(
            quitLabel,
            quitTextX, quitTextY,
            quitScale,
            glm::vec3(1.0f, 0.8f, 0.5f),
            (float)screenW,
            (float)screenH
        );

        bool overQuit =
            mouseX >= quitBx && mouseX <= quitBx + btnW &&
            uiY >= quitBy && uiY <= quitBy + btnH;

        // ---------- handle clicks ----------
        if (clicked)
        {
            if (overPlay)
            {
                engineState.SetPaused(false);
                engineState.SetPlay(true);     // game starts -> overlay disappears next frame
            }
            else if (overQuit)
            {
                // Close the whole window / game
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
        }
    }

} // namespace Framework