///////////////////////////////////////////////////////////////////////////////
///
///	\file FontSystem.cpp
/// 
/// * The FontSystem loads fonts, generates textures for each glyph, and renders text in the Unnamed Engine.
/// * It leverages the FreeType library for font loading and the OpenGL pipeline for efficient rendering.
/// * The system allows dynamic text rendering and can be easily extended for more advanced features.
/// 
///	Authors: Joshua Sim Yue Chen
///	Copyright 2024, Digipen Institute of Technology
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "FontSystem.h"
#include "ImguiManager.h"
#include <iostream>
#include <glew.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace Framework {

    FontSystem fontSystem; // global

    // Utility: load GLSL file into string
    static std::string LoadShaderFromFile(const std::string& filePath) {
        std::ifstream shaderFile(filePath);
        if (!shaderFile.is_open()) {
            std::cerr << "Failed to open shader file: " << filePath << std::endl;
            return "";
        }
        std::stringstream buffer;
        buffer << shaderFile.rdbuf();
        return buffer.str();
    }

    FontSystem::FontSystem() {
        if (FT_Init_FreeType(&ft)) {
            ImguiManager::LogError("Failed to initialize FreeType Library", "FontSystem");
            std::cerr << "Error: Failed to initialize FreeType Library" << std::endl;
        }
        else {
            ImguiManager::LogSuccess("FreeType Library initialized!", "FontSystem");
            std::cout << "INIT: FreeType Library initialized!" << std::endl;
        }

    }

    FontSystem::~FontSystem() {
        for (auto& fontPair : FontCache) {
            for (auto& charPair : fontPair.second) {
                glDeleteTextures(1, &charPair.second.TextureID);
            }
        }
        FT_Done_FreeType(ft);
    }

    void FontSystem::Initialize() {
        ImguiManager::Log("Loading font shaders...", "FontSystem");

        std::string vertexSource = LoadShaderFromFile("Assets/fontshaders/textvert.glsl");
        std::string fragmentSource = LoadShaderFromFile("Assets/fontshaders/textfrag.glsl");
        if (vertexSource.empty() || fragmentSource.empty()) {
            ImguiManager::LogError("Failed to load text shaders!", "FontSystem");
            std::cerr << "Error loading text shaders!" << std::endl;
            return;
        }

        shader.Compile(vertexSource.c_str(), fragmentSource.c_str());
        ImguiManager::LogSuccess("2D text shader compiled", "FontSystem");

        // --- Compile 3D Shader ---
        std::string vertex3DSource = LoadShaderFromFile("Assets/fontshaders/textvert3D.glsl");
        shader3D.Compile(vertex3DSource.c_str(), fragmentSource.c_str());
        ImguiManager::LogSuccess("3D text shader compiled", "FontSystem");

        // Setup VAO/VBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        ImguiManager::LogSuccess("FontSystem initialized successfully", "FontSystem");
    }

    void FontSystem::Update(float deltaTime) {
        (void)deltaTime;
    }

    bool FontSystem::LoadFont(const std::string& fontPath, int fontSize, const std::string& fontName) {
        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
            std::cerr << "Failed to load font: " << fontPath << std::endl;
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, fontSize);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        std::unordered_map<char, Character> characters;

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cerr << "Failed to load Glyph: " << c << std::endl;
                continue;
            }

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                { face->glyph->bitmap.width, face->glyph->bitmap.rows },
                { face->glyph->bitmap_left, face->glyph->bitmap_top },
                static_cast<GLuint>(face->glyph->advance.x)
            };

            characters.insert({ c, character });
        }

        FontCache[fontName] = characters;
        FT_Done_Face(face);

        return true;
    }

    void FontSystem::SetActiveFont(const std::string& fontName) {
        if (FontCache.find(fontName) != FontCache.end()) {
            activeFont = fontName;
        }
        else {
            std::cerr << "Font " << fontName << " not loaded." << std::endl;
        }
    }

    // 2D HUD text
    void FontSystem::RenderText2D(const std::string& text,
        float x, float y,
        float scale,
        glm::vec3 color,
        float screenWidth,
        float screenHeight) {


        // --- SETUP ---
        shader.Use();
        glm::mat4 projection = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);
        shader.SetUniform("textColor", color);
        shader.SetUniform("projection", projection);

        // --- ENABLE BLENDING ---
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        // Find the font in the cache safely
        auto font_it = FontCache.find(activeFont);
        if (font_it == FontCache.end()) {
            glBindVertexArray(0); // Unbind before returning
            glDisable(GL_BLEND);  // Disable blending before returning
            return;
        }
        auto& characters = font_it->second;

        // --- RENDER LOOP ---
        for (char c : text) {
            Character ch = characters[c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(xpos, ypos, 0.0f));
            model = glm::scale(model, glm::vec3(w, h, 1.0f));

            shader.SetUniform("model", model);
            float vertices[6][4] = {
                // Coords      // TexCoords
                { 0.0f, 1.0f,   0.0f, 0.0f }, // Top-left   (Tex Y is 0.0)
                { 0.0f, 0.0f,   0.0f, 1.0f }, // Bottom-left(Tex Y is 1.0)
                { 1.0f, 0.0f,   1.0f, 1.0f }, // Bottom-right(Tex Y is 1.0)

                { 0.0f, 1.0f,   0.0f, 0.0f }, // Top-left   (Tex Y is 0.0)
                { 1.0f, 0.0f,   1.0f, 1.0f }, // Bottom-right(Tex Y is 1.0)
                { 1.0f, 1.0f,   1.0f, 0.0f }  // Top-right  (Tex Y is 0.0)
            };
            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += (ch.Advance >> 6) * scale;
        }

        // --- CLEANUP ---
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // --- DISABLE BLENDING ---
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    void FontSystem::RenderText3D(const std::string& text,
        const glm::mat4& model, // This is the base position and billboard rotation
        const glm::mat4& view,
        const glm::mat4& projection,
        float scale,
        glm::vec3 color) {

        // --- SETUP & STATE CHANGES ---
        shader3D.Use();
        shader3D.SetUniform("textColor", color);
        shader3D.SetUniform("projection", projection);
        shader3D.SetUniform("view", view);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
       // glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        auto font_it = FontCache.find(activeFont);
        if (font_it == FontCache.end()) { /* ... cleanup and return ... */ }
        auto& characters = font_it->second;

        float x_cursor = 0.0f;

        // --- RENDER LOOP ---
        for (char c : text) {
            Character ch = characters.at(c);

            float xpos = x_cursor + ch.Bearing.x * scale;
            float ypos = (ch.Bearing.y - ch.Size.y) * scale;
            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            // --- THIS IS THE FIX ---
            // 1. Create a matrix for the character's local offset and size
            glm::mat4 localTransform = glm::mat4(1.0f);
            localTransform = glm::translate(localTransform, glm::vec3(xpos, ypos, 0.0f));
            localTransform = glm::scale(localTransform, glm::vec3(w, h, 1.0f));

            // 2. Combine with the main model matrix (world position + billboard)
            glm::mat4 finalCharModel = model * localTransform;

            shader3D.SetUniform("model", finalCharModel);

            // Unit quad vertices (the same as before)
            float vertices[6][4] = {
                { 0.0f, 1.0f,   0.0f, 0.0f }, { 0.0f, 0.0f,   0.0f, 1.0f }, { 1.0f, 0.0f,   1.0f, 1.0f },
                { 0.0f, 1.0f,   0.0f, 0.0f }, { 1.0f, 0.0f,   1.0f, 1.0f }, { 1.0f, 1.0f,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            x_cursor += (ch.Advance >> 6) * scale;
        }

        // --- CLEANUP ---
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

} // namespace Framework