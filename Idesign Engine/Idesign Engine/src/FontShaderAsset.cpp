///////////////////////////////////////////////////////////////////////////////
///
///	@File  : FontShaderAsset.cpp
/// @Brief : Implements the `FontShaderAsset` class, responsible for loading and
///          managing font shader assets used in text rendering. This class provides
///          functionality to deserialize and load shader source code from a specified
///          file. It allows the application to load and use shaders for text rendering
///          systems by reading the shader code from the provided file path.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "FontShaderAsset.h"

FontShaderAsset::FontShaderAsset(const std::string& filePath)
{
    DeserializeShader(filePath);
}

FontShaderAsset::~FontShaderAsset() {}

std::string FontShaderAsset::DeserializeShader(const std::string& filePath)
{
    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open()) 
    {
        std::cerr << "Failed to open shader file: " << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << shaderFile.rdbuf();
    return buffer.str();
}