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
#include <iostream>
#include <glew.h>
#include <glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace Framework {

    FontSystem fontSystem; // Define the global object

    std::string LoadShaderFromFile(const std::string& filePath) {
        std::ifstream shaderFile(filePath);
        if (!shaderFile.is_open()) {
            std::cerr << "Failed to open shader file: " << filePath << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << shaderFile.rdbuf();
        return buffer.str();
    }

    // Constructor for the FontSystem
    FontSystem::FontSystem() {
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "Error: Failed to initialize FreeType Library" << std::endl;
        }
        else {
            std::cerr << "INIT: Initialized FreeType Library!" << std::endl;
        }
    }

    // Destructor
    FontSystem::~FontSystem() {
        for (auto& fontPair : FontCache) {
            for (auto& charPair : fontPair.second) {
                glDeleteTextures(1, &charPair.second.TextureID);  // Delete the texture for each glyph
            }
        }
        FT_Done_FreeType(ft); // Free the FreeType library
    }

    // Initializes the FontSystem
    void FontSystem::Initialize() {
        std::string vertexSource = LoadShaderFromFile("src/UE_Font.vert");
        std::string fragmentSource = LoadShaderFromFile("src/UE_Font.frag");

        if (vertexSource.empty() || fragmentSource.empty()) {
            std::cerr << "Error loading shaders!" << std::endl;
            return;
        }

        shader.Compile(vertexSource.c_str(), fragmentSource.c_str());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        LoadFont("../Unnamed Studios/Assets/Font/arial.ttf", 40, "Arial");
        LoadFont("../Unnamed Studios/Assets/Font/RubikWetPaint-Regular.ttf", 40, "Rubik");
        LoadFont("../Unnamed Studios/Assets/Font/Salmon.ttf", 40, "Salmon");
        LoadFont("../Unnamed Studios/Assets/Font/SalmonBold.ttf", 40, "SalmonBold");
    }

    // Updates the FontSystem
    void FontSystem::Update(float deltaTime) {
        (void)deltaTime;
    }

    // Loads a font from a file
    bool FontSystem::LoadFont(const std::string& fontPath, int fontSize, const std::string& fontName) {
        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
            std::cerr << "Failed to load font: " << fontPath << std::endl;
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, fontSize);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction

        // Create a map to store characters for this font
        std::unordered_map<char, Character> characters;

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cerr << "Failed to load Glyph: " << c << std::endl;
                continue;
            }

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );

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

            characters.insert(std::pair<char, Character>(c, character));
        }

        FontCache[fontName] = characters; // Store characters in FontCache under fontName
        FT_Done_Face(face); // Frees face

        return true;
    }

    // Sets the active font for rendering
    void FontSystem::SetActiveFont(const std::string& fontName) {
        if (FontCache.find(fontName) != FontCache.end()) {
            activeFont = fontName;
        }
        else {
            std::cerr << "Font " << fontName << " not loaded." << std::endl;
        }
    }

    // Renders text on the screen
    void FontSystem::RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color, glm::mat4 projection) {
        shader.Use();
        shader.SetUniform("textColor", color);
        shader.SetUniform("projection", projection);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        auto& characters = FontCache[activeFont];

        for (auto c = text.begin(); c != text.end(); ++c) {
            Character ch = characters[*c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Bearing.y * scale);
            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 1.0f },
                { xpos,     ypos,       0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 0.0f },

                { xpos,     ypos + h,   0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 0.0f },
                { xpos + w, ypos + h,   1.0f, 1.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += (ch.Advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace Framework
