///////////////////////////////////////////////////////////////////////////////
///
///	@File  : FontShaderAsset.h
/// @Brief : This file defines the `FontShaderAsset` class, which is responsible
///          for loading and managing font shader assets used in text rendering.
///          It provides a method to deserialize and load shader source code from
///          a given file, allowing the use of shaders in font rendering systems.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _FONT_SHADER_ASSET_H_
#define _FONT_SHADER_ASSET_H_
#include "pch.h"
#include "JsonSerialize.h"

/**
 * @class FontShaderAsset
 * @brief Manages font shader assets, including loading and deserializing shader code.
 */
class FontShaderAsset
{
public:

	/**
	 * @brief Constructs a FontShaderAsset and loads a font shader from the specified file.
	 * @param filePath Path to the shader file.
	 */
	FontShaderAsset(const std::string& filePath);

	/**
	 * @brief Destructor for the FontShaderAsset class, releasing resources.
	 */
	~FontShaderAsset();

	/**
	 * @brief Loads and deserializes a font shader from the specified file path.
	 * @param filePath Path to the shader file.
	 * @return A string containing the shader source code.
	 */
	std::string DeserializeShader(const std::string& filePath);
private:
};
#endif // !_FONT_SHADER_ASSET_H_