///////////////////////////////////////////////////////////////////////////////
///
///	@File  : TextureAsset.h
/// @Brief : Defines the TextureAsset class, responsible for managing texture
///          assets in the application. This includes loading textures from files,
///          serializing/deserializing texture data, and providing access to texture
///          properties such as names, paths, and OpenGL texture IDs.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _TEXTURE_ASSET_H_
#define _TEXTURE_ASSET_H_
#include "pch.h"
#include "JsonSerialize.h"
#include <glew.h>

/**
 * @class TextureAsset
 * @brief Manages texture assets, including loading, serializing, and providing access
 *        to texture properties such as names, paths, and OpenGL texture IDs.
 */
class TextureAsset
{
public:

	/**
	 * @struct Texture
	 * @brief Represents a single texture with a name and path.
	 */
	struct Texture
	{
		std::string name;
		std::string path;
		GLuint textureID = 0;  // Store texture ID once loaded
	};

	/**
	 * @brief Default constructor for TextureAsset.
	 */
	TextureAsset();
	
	/**
	 * @brief Constructs a TextureAsset and loads texture data from a file.
	 * @param filePath Path to the texture configuration file.
	 */
	TextureAsset(const std::string& filePath);

	/**
	 * @brief Destructor for the TextureAsset class.
	 */
	~TextureAsset();

	/**
	 * @brief Deserializes texture data from a file.
	 * @param filePath Path to the texture configuration file.
	 */
	void Deserialize(const std::string& filePath);

	/**
	 * @brief Serializes texture data to a file.
	 * @param filePath Path where the texture data will be saved.
	 */
	void Serialize(const std::string& filePath);

	/**
	 * @brief Retrieves the names of all textures.
	 * @return A vector of texture names.
	 */
	std::vector<std::string> GetNames() const;

	/**
	 * @brief Retrieves the path for a specific texture type.
	 * @param textureType The type or name of the texture.
	 * @return The path to the texture file.
	 */
	std::string GetPath(const std::string& textureType) const;

	/**
	 * @brief Retrieves the OpenGL texture ID associated with the texture.
	 * @return The OpenGL texture ID.
	 */
	GLuint GetTextureID(const std::string& textureName) const;

	/**
	 * @brief Sets the OpenGL texture ID.
	 * @param id The OpenGL texture ID.
	 */
	void SetTextureID(const std::string& textureName, GLuint id);

	/**
	 * @brief Sets the name of a texture at a specific index.
	 * @param index The index of the texture.
	 * @param newName The new name to assign to the texture.
	 */
	void SetName(const std::string& oldName, const std::string& newName);

	/**
	 * @brief Adds a new texture to the texture asset.
	 * @param name The name of the new texture.
	 * @param path The path to the texture file.
	 */
	void AddTexture(const std::string& name, const std::string& path);

	// Method to get a texture by name (if it exists)
	Texture* GetTexture(const std::string& textureName);

	// Method to update a texture in the map
	void UpdateTextureNameAndPath(const std::string& oldName, const std::string& newName, const std::string& newPath);

	void RemoveTexture(const std::string& textureName);

	// Map to store textures by their names
	std::map<std::string, Texture>& GetAllTextureMap()
	{
		return texturesMap;
	}

private:
	
	// Map to store textures by their names
	std::map<std::string, Texture> texturesMap;
};
#endif // !_TEXTURE_ASSET_H_