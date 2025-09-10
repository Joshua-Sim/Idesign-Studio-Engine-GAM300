#pragma once

#include "pch.h"
#include "system.h"
#include <unordered_map>
#include <string>
#include <ft2build.h>
#include <glew.h>
#include <glm.hpp>
#include <map>
#include "FontShader.h"
#include <fstream>
#include <sstream>
#include <gtc/matrix_transform.hpp>
#include FT_FREETYPE_H

namespace Framework {

    struct Character {
        GLuint TextureID;
        glm::ivec2 Size;
        glm::ivec2 Bearing;
        GLuint Advance;
    };

    struct Font {
        FT_Face face;
        int size;
    };

    class FontSystem : public ISystem {
    public:
        FontSystem();
        ~FontSystem();
        void Initialize();
        void Update(float deltaTime) override;
        std::string GetName() override { return "FontSystem"; }

        bool LoadFont(const std::string& fontPath, int fontSize, const std::string& fontName);
        void RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color, glm::mat4 projection);
        void SetActiveFont(const std::string& fontName);

    private:
        FT_Library ft;
        std::unordered_map<std::string, std::unordered_map<char, Character>> FontCache;
        std::string activeFont;

        GLuint VAO{}, VBO{};
        FontShader shader;
    };

    // Declare the global FontSystem pointer
    extern FontSystem fontSystem; // Declare as an object

} // namespace Framework
