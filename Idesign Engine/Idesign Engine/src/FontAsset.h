///////////////////////////////////////////////////////////////////////////////
///
///	@File  : FontAsset.h
/// @Brief : Defines the FontAsset class responsible for managing font resources.
///          This includes loading font files, deserializing font data, and providing
///          access to character information for text rendering, using FreeType for
///          font rendering and glyph generation.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _FONT_ASSET_H_
#define _FONT_ASSET_H_
#include "pch.h"
#include "JsonSerialize.h"
#include "glm.hpp"
#include <glew.h>
#include "freetype/freetype.h"

/**
 * @class FontAsset
 * @brief Manages font assets by loading font files, deserializing font data,
 *        and providing access to character glyphs for rendering using FreeType.
 */
class FontAsset
{
public:

    /**
     * @struct Character
     * @brief Represents a single character's glyph data.
     *        Contains information such as texture ID, size, bearing, and advance.
     */
    struct Character 
    {
        GLuint TextureID;   // ID handle of the glyph texture
        glm::ivec2 Size;    // Size of the glyph (width and height)
        glm::ivec2 Bearing; // Offset from baseline to left/top of glyph
        GLuint Advance;     // Offset to advance to the next glyph
    };

    /**
     * @struct Font
     * @brief Represents a font, storing the FreeType face object and font size.
     */
    struct Font 
    {
        FT_Face face;   // FreeType face object
        int size;       // Font size
    };

    /**
     * @brief Constructs a FontAsset and loads a font from a file.
     * @param filePath Path to the font file.
     * @param fontSize The size of the font to load.
     */
	FontAsset(const std::string& filePath, int fontSize);

    /**
     * @brief Destructor for the FontAsset class, releasing resources.
     */
    ~FontAsset();

    /**
     * @brief Deserializes and loads font data from a file using FreeType.
     * @param fontPath Path to the font file.
     * @param fontSize Size of the font to load.
     * @return True if the font was successfully loaded, false otherwise.
     */
	bool DeserializeFont(const std::string& fontPath, int fontSize);
    
    /**
     * @brief Retrieves the map of loaded characters for rendering.
     * @return A constant reference to an unordered_map of characters.
     */
    const std::unordered_map<char, Character>& GetCharacters() const { return Characters; }

private:
    FT_Library ft;                                  // FreeType library handle
	std::map<std::string, Font> loadedFonts;        // Map of loaded fonts
    std::unordered_map<char, Character> Characters; // Map of characters for rendering
};
#endif // _FONT_ASSET_H_