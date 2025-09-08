///////////////////////////////////////////////////////////////////////////////
///
///	@File  : FontAsset.cpp
/// @Brief : Implements the FontAsset class for managing font resources in the application.
///          This class is responsible for loading font files, deserializing font data, 
///          and providing access to character information for text rendering. It uses 
///          the FreeType library to handle font rendering and glyph generation. The class 
///          initializes FreeType, loads font files, generates textures for each glyph, 
///          and stores the character information in a map for efficient access during 
///          text rendering. Upon destruction, it also cleans up any allocated resources, 
///          including textures and FreeType data.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "FontAsset.h"

FontAsset::FontAsset(const std::string& filePath, int fontSize)
{
    DeserializeFont(filePath, fontSize);
}

FontAsset::~FontAsset()
{
    for (auto& pair : Characters) 
    {
            glDeleteTextures(1, &pair.second.TextureID);  // Delete the texture for each glyph
    }
    FT_Done_FreeType(ft);
}

bool FontAsset::DeserializeFont(const std::string& fontPath, int fontSize)
{
    if (FT_Init_FreeType(&ft)) 
    {
        std::cerr << "Could not init FreeType Library" << std::endl;
        return false;
    }

    std::cout << "FreeType Library Initialised in FontAsset.cpp" << std::endl;

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) 
    {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++) 
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) 
        {
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

        Character character = 
        {
            texture,
            { face->glyph->bitmap.width, face->glyph->bitmap.rows },
            { face->glyph->bitmap_left, face->glyph->bitmap_top },
            static_cast<GLuint>(face->glyph->advance.x)
        };

        Characters.insert(std::pair<char, Character>(c, character));
    }
    FT_Done_Face(face); // frees face
    return true;
}